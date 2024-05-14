#include <regex>
#include <stdlib.h>
#include <openssl/md5.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <getopt.h>
#include <errno.h>
#include <algorithm>
#include <vector>
#include <cstring>
#include <string>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <set>
#include <iostream>

#include "../include/pop3_msg.hh"
#include "../include/io_helper.hh"

#include "KVSClient.hpp"

extern KVSClient kvsClient;


void getMboxContent(std::string& mboxContent, const std::string& user, const std::string& mutexId)
{
  std::string rowKey = user + ".mbox";
  // std::cout << "rowKey: " << rowKey << std::endl;
  // std::cout << "mutexId: " << mutexId << std::endl;
  mboxContent = "";
  std::vector<std::string> colKeys;
  if (!kvsClient.GetColsInRow(rowKey, colKeys, mutexId))
  {
    std::cerr << "GetColsInRow failed" << std::endl;
    return;
  }
  
  for (const auto& colKey: colKeys)
  {
    // std::cout << "colKey: " << colKey << std::endl;
    std::string value;
    if (!kvsClient.Get(rowKey, colKey, value, mutexId))
    {
      std::cerr << "Get failed" << std::endl;
      return;
    }
    mboxContent.append(value);
  }
}


/**
 * @brief Processes the USER command in a POP3 context, attempting to set the current session's username based on the client's input.
 *        This function checks if a user is already authenticated in the current session. If so, it sends an error message indicating
 *        that a user has already been authenticated. If no user is authenticated yet, it searches the provided list of valid users
 *        for the username supplied in the argument. If the username is found, it is accepted, and the session's username pointer is set
 *        to a dynamically allocated copy of the argument. If the username is not found in the list of valid users, an error message is sent
 *        to the client indicating that the user was not found.
 *
 * @param sock The socket descriptor used to communicate with the client. Responses are sent through this socket.
 * @param argument The username provided by the client. This is the value that the client wishes to authenticate with.
 * @param username A pointer to a pointer to char, representing the current session's username. If authentication is successful,
 *                 this pointer is set to point to a dynamically allocated string containing the authenticated username.
 * @param verbose A boolean flag indicating whether additional logging should be performed. If true, the function may log additional
 *                details about its operation.
 *
 * @return void The function does not return a value. It sends responses directly to the client through the socket based on the outcome
 *              of the username authentication process.
 */
void processUserCommand(int sock, char *argument, char **username, bool verbose)
{
  if (*username != nullptr)
  {
    err_code(sock, verbose, "A user already authenticated");
    return;
  }
  std::string argumentStr(argument);
  std::string value;
  if (!kvsClient.Get("accounts", argumentStr, value, "LOCK_BYPASS"))
  {
    char message[256];
    sprintf(message, "User %s not found", argument);
    err_code(sock, verbose, message);
  }
  else
  {
    char message[256];
    sprintf(message, "User %s accepted", argument);
    ok_code(sock, verbose, message);
    *username = strdup(argument);
  }
}

/**
 * @brief Processes the PASS command in a POP3 context, attempting to authenticate a user by validating the provided password.
 *        If the password matches the expected value ("cis505"), the function attempts to open and exclusively lock the user's mailbox file.
 *        The mailbox file is determined by concatenating the user's provided username with the base file path and ".mbox" extension.
 *        Successful locking indicates that no other process is accessing the mailbox, allowing the user to proceed with mail retrieval and manipulation.
 *        This function sets the `loggedIn` flag to true upon successful authentication and file locking, and opens the file descriptor `fd` for the mailbox file.
 *        If the password is incorrect or the mailbox file cannot be locked (e.g., it's already in use), appropriate error messages are sent to the client.
 *
 * @param sock The socket descriptor to send responses to the client.
 * @param argument The password provided by the client for authentication.
 * @param loggedIn A pointer to a boolean flag that indicates whether the user is successfully logged in.
 * @param user The username of the client attempting to authenticate. Used to determine the mailbox file path.
 * @param file_path The base path where mailbox files are stored. The user's mailbox filename is derived by appending the username and ".mbox" to this path.
 * @param fd A pointer to an integer that will store the file descriptor of the opened mailbox file upon successful locking.
 * @param verbose A boolean flag for verbose logging. If true, additional details are logged.
 * @param mutexId A string that will store the mutex id for the mailbox file upon successful locking.
 * 
 * @return void The function does not return a value. It communicates success or failure to the client through the provided socket descriptor.
 */
void processPassCommand(int sock, char *argument, bool *loggedIn, char **user, bool verbose, std::string& mutexId)
{
  std::string value;
  std::string username(*user);
  if (!kvsClient.Get("accounts", *user, value, "LOCK_BYPASS"))
  {
    std::cerr << "accounts Get failed in processPassCommand()" << std::endl;
    err_code(sock, verbose, "User not found");
    return;
  }
  if (value == std::string(argument))
  {
    std::string mboxPath = std::string(*user) + ".mbox";

    bool mutexAcquired = false;
    do { // keep trying until mutex is acquired
      if (verbose) {
        std::cout << "Try locking " << mboxPath << std::endl;
      }
      mutexAcquired = kvsClient.SetNX(mboxPath, mutexId);
      if (!mutexAcquired) sleep(1);
    } while (!mutexAcquired);

    if (verbose) {
      std::cout << "Lock acquired for " << mboxPath << " mutexId: " << mutexId << std::endl;
    }

    // If here, the file is successfully locked
    ok_code(sock, verbose, "maildrop locked and ready");
    *loggedIn = true;
  }
  else
  {
    *user = NULL;
    err_code(sock, verbose, "invalid password");
  }
}

/**
 * @brief Calculates the total size of all messages in a collection.
 *        This function iterates over a vector of email messages, each represented as a string,
 *        and sums their sizes to determine the total size in bytes. The size of each message
 *        is considered to be its length in characters, assuming each character is one byte.
 *        This function is useful for determining the cumulative size of email messages,
 *        for example, to report the total size of messages in a maildrop.
 *
 * @param messages A vector containing the email messages as strings. Each string in the vector
 *                 represents one email message, and its size is measured by the number of characters it contains.
 * @return size_t The total size of all messages in the vector, measured in bytes. This is the sum
 *                of the sizes of all individual messages.
 */
size_t calculateTotalSize(const std::vector<std::string> &messages)
{
  size_t totalSize = 0;
  for (const auto &message : messages)
  {
    totalSize += message.size(); // Add the size of each message in bytes
  }
  return totalSize;
}

/**
 * @brief Processes the STAT command for a POP3 server, calculating and responding with the total number of
 *        non-deleted messages and the combined size of these messages in the mailbox. The function seeks to the
 *        start of the mailbox file, reads its entire content, and then splits it into individual messages based on
 *        the mbox format. It then filters out messages marked as deleted by checking against the `deletedMessage` set.
 *        The size of each non-deleted message is calculated, and these sizes are summed to provide the total mailbox size
 *        for non-deleted messages. The response to the client includes the count of non-deleted messages and this total size.
 *
 * @param sock The socket descriptor used to communicate responses back to the client.
 * @param deletedMessage A set of message numbers that have been marked as deleted. Messages in this set are excluded from the count and size calculation.
 * @param verbose A boolean flag indicating whether to log additional information for debugging or verbose output.
 *
 * @return void The function does not return a value. Instead, it sends the calculated statistics directly to the client through the provided socket descriptor.
 */
void processStatCommand(int sock, char* user, std::set<unsigned int> &deletedMessage, bool verbose, const std::string& mutexId)
{
  if (mutexId == "-")
  {
    err_code(sock, verbose, "maildrop not opened");
    return;
  }

  std::string mboxContent;
  getMboxContent(mboxContent, std::string(user), mutexId);

  // Split the mbox content into messages
  std::vector<std::string> messages = splitMessages(mboxContent, false);
  for (auto it = deletedMessage.begin(); it != deletedMessage.end(); ++it)
  {
    messages[*it - 1] = "";
  }
  char response[256];
  snprintf(response, sizeof(response), "%d %ld", messages.size() - deletedMessage.size(), calculateTotalSize(messages));
  ok_code(sock, verbose, response);
}

/**
 * @brief Processes the LIST command in a POP3 context, listing message sizes or a specific message size.
 *        If an argument is provided, the function returns the size of the specified message.
 *        Without an argument, it lists the sizes of all messages that have not been marked as deleted.
 *        The function seeks to the start of the mailbox file, reads its content, and splits it into messages.
 *        It checks whether each message is marked as deleted by looking up its number in the deletedMessage set.
 *        For a specific message request, it validates the message number and checks if it's marked as deleted.
 *        For the general listing, it iterates through all messages, excluding deleted ones, and sends their sizes.
 *
 * @param sock The socket descriptor to send responses to the client.
 * @param fd The file descriptor of the mailbox file. If it's -1, indicates the mailbox is not opened.
 * @param argument An optional argument specifying the message number to list. If NULL, lists all messages.
 * @param deletedMessage A set containing the numbers of messages marked as deleted.
 * @param verbose A boolean flag for verbose logging. If true, additional details are logged.
 *
 * @return void The function does not return a value but sends responses directly to the client via the socket.
 */
void processListCommand(int sock, char* user,  char *argument, std::set<unsigned int> &deletedMessage, bool verbose, const std::string& mutexId)
{
  if (mutexId == "-")
  {
    err_code(sock, verbose, "maildrop not opened");
    return;
  }

  std::string mboxContent;
  getMboxContent(mboxContent, std::string(user), mutexId);

  // Split the mbox content into messages
  std::vector<std::string> messages = splitMessages(mboxContent, false);
  if (argument != NULL)
  {
    // LIST <id>
    int id = atoi(argument);
    if (id < 1 || id > messages.size())
    {
      char response[256];
      snprintf(response, sizeof(response), "%d no such message, only %d messages in maildrop", id, messages.size());
      err_code(sock, verbose, response);
      return;
    }
    if (deletedMessage.find(id) != deletedMessage.end())
    {
      char response[256];
      snprintf(response, sizeof(response), "%d message %d already deleted", id, id);
      err_code(sock, verbose, response);
      return;
    }
    char response[256];
    snprintf(response, sizeof(response), "%d %ld", id, messages[id - 1].size());
    ok_code(sock, verbose, response);
  }
  else
  {
    // LIST
    ok_code(sock, verbose, "");
    char response[256];
    for (size_t i = 0; i < messages.size(); ++i)
    {
      if (deletedMessage.find(i + 1) != deletedMessage.end())
      {
        continue;
      }
      snprintf(response, sizeof(response), "%d %ld\r\n", i + 1, messages[i].size());
      msg_code(sock, verbose, response);
    }
    msg_code(sock, verbose, ".\r\n");
  }
}

/**
 * @brief Computes the MD5 digest of the given data and stores the result in the provided buffer.
 *        This function initializes an MD5 context, processes the input data to compute the digest,
 *        and then finalizes the computation, storing the digest in the specified buffer. The buffer
 *        must be pre-allocated and have a size of at least MD5_DIGEST_LENGTH bytes to hold the
 *        128-bit MD5 hash value.
 *
 * @param data Pointer to the data for which the MD5 digest is to be computed.
 * @param dataLengthBytes The length of the data in bytes.
 * @param digestBuffer Pointer to the buffer where the computed digest will be stored. This buffer
 *                     must be at least MD5_DIGEST_LENGTH bytes in size.
 *
 * @return void The function does not return a value. The computed MD5 digest is stored directly in
 *              the buffer provided via the digestBuffer parameter.
 */
void computeDigest(char *data, int dataLengthBytes, unsigned char *digestBuffer)
{
  /* The digest will be written to digestBuffer, which must be at least MD5_DIGEST_LENGTH bytes long */

  MD5_CTX c;
  MD5_Init(&c);
  MD5_Update(&c, data, dataLengthBytes);
  MD5_Final(digestBuffer, &c);
}

/**
 * @brief Converts a binary hash value into a hexadecimal string representation.
 *        This function iterates over each byte of the hash, formats it as two hexadecimal
 *        digits, and appends these digits to a string. The resulting string contains the
 *        hexadecimal representation of the binary hash, suitable for display or storage
 *        in text formats. The function ensures proper formatting with leading zeros for
 *        bytes less than 0x10.
 *
 * @param hash Pointer to the binary hash data to be converted.
 * @param length The number of bytes in the hash data.
 * @return std::string The hexadecimal string representation of the hash.
 */
std::string hashToString(const unsigned char *hash, size_t length)
{
  std::string hashStr;
  for (size_t i = 0; i < length; ++i)
  {
    char hex[3]; // Two hexadecimal digits and a null terminator
    snprintf(hex, sizeof(hex), "%02x", hash[i]);
    hashStr.append(hex);
  }
  return hashStr;
}

/**
 * @brief Processes the UIDL command in a POP3 context, generating and sending unique identifiers (UIDLs) for each message in the maildrop.
 *        If an argument is provided, the function returns the UIDL for the specified message. Without an argument, it lists the UIDLs for
 *        all messages not marked as deleted. This function reads the entire mailbox content, splits it into individual messages, and then
 *        computes an MD5 digest for each message as its unique identifier. Deleted messages are skipped, and UIDLs are only generated for
 *        existing (non-deleted) messages. The UIDL for a message is composed of its sequence number in the maildrop and its MD5 hash.
 *
 * @param sock The socket descriptor used to communicate with the client. Responses, including UIDLs, are sent through this socket.
 * @param fd The file descriptor for the opened mailbox file. A value of -1 indicates the mailbox is not currently opened.
 * @param argument An optional argument specifying the message number for which to return the UIDL. If NULL, UIDLs for all non-deleted messages are listed.
 * @param deletedMessage A set containing the numbers of messages marked as deleted. These messages are excluded from the UIDL listing.
 * @param verbose A boolean flag indicating whether additional logging should be performed. If true, the function may log additional details about its operation.
 *
 * @return void The function does not return a value. It sends the computed UIDLs directly to the client through the socket based on the presence
 *              of an argument and the status of messages in the maildrop.
 */
void processUidlCommand(int sock, char* user, char *argument, std::set<unsigned int> &deletedMessage, bool verbose, const std::string& mutexId)
{
  if (mutexId == "-")
  {
    err_code(sock, verbose, "maildrop not opened");
    return;
  }

  std::string mboxContent;
  getMboxContent(mboxContent, std::string(user), mutexId);

  // Split the mbox content into messages
  std::vector<std::string> messages = splitMessages(mboxContent, true);

  // print argument
  if (argument != NULL)
  {
    // UIDL <id>
    int id = atoi(argument);
    if (id < 1 || id > messages.size())
    {
      char response[256];
      snprintf(response, sizeof(response), "%d no such message, only %d messages in maildrop", id, messages.size());
      err_code(sock, verbose, response);
      return;
    }
    if (deletedMessage.find(id) != deletedMessage.end())
    {
      char response[256];
      snprintf(response, sizeof(response), "%d message %d already deleted", id, id);
      err_code(sock, verbose, response);
      return;
    }
    unsigned char hash[MD5_DIGEST_LENGTH];
    computeDigest((char *)messages[id - 1].c_str(), messages[id - 1].size(), hash);
    std::string hashStr = hashToString(hash, MD5_DIGEST_LENGTH);

    char response[256];
    snprintf(response, sizeof(response), "%d %s", id, hashStr.c_str());
    ok_code(sock, verbose, response);
  }
  else
  {
    // UIDL
    ok_code(sock, verbose, "");
    char response[256];
    for (size_t i = 0; i < messages.size(); ++i)
    {
      if (deletedMessage.find(i + 1) != deletedMessage.end())
      {
        continue;
      }
      unsigned char hash[MD5_DIGEST_LENGTH];
      computeDigest((char *)messages[i].c_str(), messages[i].size(), hash);

      std::string hashStr = hashToString(hash, MD5_DIGEST_LENGTH);

      snprintf(response, sizeof(response), "%d %s\r\n", i + 1, hashStr.c_str());
      msg_code(sock, verbose, response);
    }
    msg_code(sock, verbose, ".\r\n");
  }
}

/**
 * @brief Processes the RETR command in a POP3 context, sending the content of a specified message to the client.
 *        This command retrieves the full text of a message identified by its message number, provided it has not been marked as deleted.
 *        The function first checks for valid input and mailbox accessibility, then reads the entire mailbox content to split it into
 *        individual messages. It validates the specified message number against the total number of messages and whether the message
 *        has been marked as deleted. If valid, the function sends the size of the message followed by its content. The operation concludes
 *        with a single period on a line by itself, indicating the end of the message content.
 *
 * @param sock The socket descriptor used to communicate responses and message content to the client.
 * @param fd The file descriptor for the opened mailbox file. A value of -1 indicates that the mailbox is not open, triggering an error response.
 * @param argument The command argument, which should be the message number as a string. If NULL, an error is reported for the missing message number.
 * @param deletedMessages A set containing the numbers of messages marked as deleted. Messages in this set are not retrievable with the RETR command.
 * @param verbose A boolean flag indicating whether to log additional information for debugging purposes. If true, additional operation details are logged.
 *
 * @return void The function does not return a value. Instead, it directly communicates with the client, sending the requested message or reporting errors.
 */
void processRetrCommand(int sock, char* user, char *argument, std::set<unsigned int> &deletedMessages, bool verbose, const std::string& mutexId)
{
  if (argument == NULL)
  {
    err_code(sock, verbose, "missing message number");
    return;
  }

  if (mutexId == "-")
  {
    err_code(sock, verbose, "maildrop not opened");
    return;
  }

  std::string mboxContent;
  getMboxContent(mboxContent, std::string(user), mutexId);

  // Split the mbox content into messages
  std::vector<std::string> messages = splitMessages(mboxContent, false);

  int id = atoi(argument);
  if (id < 1 || id > messages.size())
  {
    char response[256];
    snprintf(response, sizeof(response), "no such message");
    err_code(sock, verbose, response);
    return;
  }
  if (deletedMessages.find(id) != deletedMessages.end())
  {
    char response[256];
    snprintf(response, sizeof(response), "message %d already deleted", id);
    err_code(sock, verbose, response);
    return;
  }
  char response[256];
  snprintf(response, sizeof(response), "%d octets", messages[id - 1].size());
  ok_code(sock, verbose, response);

  msg_code(sock, verbose, messages[id - 1].c_str());
  msg_code(sock, verbose, ".\r\n");
}

/**
 * @brief Processes the DELE command in a POP3 context, marking a specified message as deleted.
 *        The DELE command allows a client to mark a message in the mailbox for deletion. This function
 *        checks if the provided message number is valid and not already marked as deleted. If valid, the
 *        message is marked as deleted by adding its number to the set of deleted messages. Messages marked
 *        as deleted are not physically removed until the QUIT command is processed successfully. This function
 *        ensures the mailbox file is accessible and seeks to the start before processing. It reads the entire
 *        mailbox content, splits it into individual messages, and checks the specified message against the
 *        collection of messages to validate its existence and deletion status.
 *
 * @param sock The socket descriptor used to communicate with the client, sending responses about the outcome
 *             of the DELE command.
 * @param fd The file descriptor for the mailbox file. A value of -1 indicates that the mailbox is not open,
 *           and an error is reported.
 * @param argument The command argument provided by the client, expected to be the message number as a string.
 *                 If NULL, an error indicating a missing message number is reported.
 * @param deletedMessages A set of message numbers that have been marked as deleted. If the specified message
 *                        is already in this set, an error is reported. Otherwise, the message is added to the set.
 * @param verbose A boolean flag indicating whether additional operation details should be logged for debugging
 *                purposes. If true, verbose logging is enabled.
 *
 * @return void The function does not return a value. It communicates directly with the client through the
 *              provided socket, sending appropriate responses based on the DELE command's processing outcome.
 */
void processDeleCommand(int sock, char* user, char *argument, std::set<unsigned int> &deletedMessages, bool verbose, const std::string& mutexId)
{
  if (argument == NULL)
  {
    err_code(sock, verbose, "missing message number");
    return;
  }

  if (mutexId == "-")
  {
    err_code(sock, verbose, "maildrop not opened");
    return;
  }

  std::string mboxContent;
  getMboxContent(mboxContent, std::string(user), mutexId);

  // Split the mbox content into messages
  std::vector<std::string> messages = splitMessages(mboxContent, false);

  int id = atoi(argument);
  if (id < 1 || id > messages.size())
  {
    char response[256];
    snprintf(response, sizeof(response), "no such message");
    err_code(sock, verbose, response);
    return;
  }
  if (deletedMessages.find(id) != deletedMessages.end())
  {
    char response[256];
    snprintf(response, sizeof(response), "message %d already deleted", id);
    err_code(sock, verbose, response);
    return;
  }
  deletedMessages.insert(id);
  char response[256];
  snprintf(response, sizeof(response), "message %d deleted", id);
  ok_code(sock, verbose, response);
}

/**
 * @brief Processes the RSET command in a POP3 context, which undeletes all messages previously marked for deletion.
 *        The RSET command resets the state of the maildrop by clearing the set of deleted messages, effectively
 *        restoring them to their undeleted state. This function sends a confirmation response to the client indicating
 *        the number of messages restored. Following the execution of this command, the maildrop is considered to have
 *        no messages marked as deleted, allowing the client to interact with all messages as if no DELE commands had
 *        been issued in the session.
 *
 * @param sock The socket descriptor used to communicate with the client. Responses are sent through this socket.
 * @param deletedMessages A reference to a set containing the numbers of messages marked as deleted. This set is cleared,
 *                        indicating that no messages are considered deleted after this command's execution.
 * @param verbose A boolean flag indicating whether additional information should be logged. If true, the function may
 *                log additional details about the RSET command's processing.
 *
 * @return void The function does not return a value. It sends a response to the client through the socket, detailing
 *              the outcome of the RSET command.
 */
void processRsetCommand(int sock, char* user, std::set<unsigned int> &deletedMessages, bool verbose, const std::string& mutexId)
{
  char response[256];
  snprintf(response, sizeof(response), "maildrop has %d messages restored", deletedMessages.size());
  ok_code(sock, verbose, response);
  deletedMessages.clear();
}

/**
 * @brief Helper function for the QUIT command, responsible for permanently removing messages marked as deleted
 *        from the mailbox file. It reads the entire mailbox content, filters out messages that are marked as deleted,
 *        and writes the remaining messages back to the mailbox file. This operation ensures that only non-deleted
 *        messages are retained in the maildrop. If any part of the process fails (e.g., seeking the file, truncating it,
 *        or writing the new content), an error message is sent to the client, and the function returns false to indicate
 *        failure. Successful execution results in the updated mailbox content being saved, and the function returns true.
 *
 * @param sock The socket descriptor used to communicate with the client. This is used for sending error messages
 *             if the operation encounters issues.
 * @param fd The file descriptor for the mailbox file. A value of -1 indicates that the mailbox is not open, which
 *           immediately results in an error response.
 * @param deletedMessages A reference to a set containing the numbers of messages marked as deleted. These messages
 *                        are excluded from the new mailbox content.
 * @param verbose A boolean flag indicating whether additional information should be logged. If true, error conditions
 *                may trigger verbose logging to aid in debugging.
 *
 * @return bool Returns true if the operation to remove deleted messages and update the mailbox file is successful.
 *              Returns false if any part of the operation fails, indicating that some or all deleted messages
 *              may not have been properly removed.
 */
bool processQuitCommandHelper(int sock, char* user, std::set<unsigned int> &deletedMessages, bool verbose, const std::string& mutexId)
{
  if (mutexId == "-")
  {
    err_code(sock, verbose, "some deleted messages not removed due to lack of mutexId");
    return false;
  }
  std::string rowKey = std::string(user) + ".mbox";

  std::vector<std::string> colKeys;
  if (!kvsClient.GetColsInRow(rowKey, colKeys, mutexId))
  {
    std::cerr << "GetColsInRow failed in processQuitCommandHelper" << std::endl;
    err_code(sock, verbose, "some deleted messages not removed due to GetColsInRow failure");
    return false;
  }
  
  // remove deleted messages
  for (int i = 0 ;i < colKeys.size(); ++i)
  {
    std::string colKey = colKeys[i];
    if (deletedMessages.find(i + 1) != deletedMessages.end()) // this message shall be removed
    {
      if (!kvsClient.Delete(rowKey, colKey, mutexId))
      {
        err_code(sock, verbose, "some deleted messages not removed due to deletion failure");
        if (!kvsClient.Del(rowKey, mutexId)) // relase the lock when fail
        {
          std::cerr << "Del failed in processQuitCommandHelper" << std::endl;
        } else if (verbose) {
          std::cout << "Lock release success in processQuitCommandHelper, mutexId: " << mutexId  << std::endl;
        }
        return false;
      }
    }
  }
  if (!kvsClient.Del(rowKey, mutexId)) // relase the lock when success
  {
    std::cerr << "Del failed in processQuitCommandHelper" << std::endl;
    return false;
  } else if (verbose) {
    std::cout << "Lock release success in processQuitCommandHelper, mutextId: " << mutexId << std::endl;
  }
  return true;
}

/**
 * @brief Executes the QUIT command in a POP3 session, finalizing the session by removing messages marked for deletion,
 *        unlocking the mailbox file, and closing the connection. This function relies on the processQuitCommandHelper
 *        to perform the actual message removal process. If the helper function successfully removes the marked messages
 *        and updates the mailbox, a confirmation response is sent to the client indicating that the POP3 server is signing off
 *        and the number of messages deleted. Regardless of the helper function's success, this function then proceeds to unlock
 *        the mailbox file and close the file descriptor, effectively releasing any resources associated with the mailbox for
 *        this session. The TCP connection to the client is expected to be closed after this command completes.
 *
 * @param sock The socket descriptor used for communication with the client. This is where the final response is sent.
 * @param fd The file descriptor of the mailbox file. This is used for file operations such as unlocking and closing the file.
 * @param deletedMessages A set containing the numbers of messages that were marked as deleted during the session. This set is
 *                        used by the helper function to determine which messages to remove.
 * @param verbose A boolean flag indicating whether additional logging should be performed. If true, the function may log
 *                additional details about the QUIT command's execution.
 *
 * @return void The function does not return a value. It concludes the POP3 session by sending a final response to the client,
 *              releasing the mailbox file lock, and closing the file descriptor.
 */
void processQuitCommand(int sock, char* user, std::set<unsigned int> &deletedMessages, bool verbose, std::string& mutexId)
{
  if (processQuitCommandHelper(sock, user, deletedMessages, verbose, mutexId))
  {
    char response[256];
    snprintf(response, sizeof(response), "POP3 server signing off (%d messages deleted)", deletedMessages.size());
    ok_code(sock, verbose, response);
  }
}
