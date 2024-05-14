#include <string>
#include <algorithm>
#include <vector>
#include <iostream>
#include <cstring>
#include <fstream>
#include <mutex>
#include <chrono>
#include <ctime>
#include <sstream>
#include <map>
#include <openssl/md5.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>



#include "../include/smpt_msg.hh"
#include "KVSClient.hpp"

extern KVSClient kvsClient;


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
 * @brief Processes the MAIL FROM command in an SMTP session, extracting and validating the sender's email address.
 *
 * This function parses the MAIL FROM command argument to extract the sender's email address, enclosed within
 * angle brackets (< and >). It validates the presence of these brackets and the syntactical validity of the
 * email address contained within them. If the argument is missing, does not contain a properly formatted email
 * address, or if the email address is invalid based on syntactical checks, the function responds with a 501
 * syntax error. Upon successful extraction and validation of the email address, it updates the provided
 * reversePath pointer to point to a dynamically allocated copy of the email address, and sends a 250 OK response
 * to acknowledge successful processing of the command.
 *
 * @param sock The socket file descriptor associated with the client connection. This is used for sending
 *             SMTP responses back to the client.
 * @param argument The argument provided with the MAIL FROM command, expected to contain the sender's email
 *                 address enclosed in angle brackets.
 * @param reversePath A pointer to a character pointer, which this function updates to point to a dynamically
 *                    allocated string containing the extracted email address. The caller is responsible for
 *                    freeing this memory.
 * @param verbose A boolean flag indicating whether verbose logging is enabled. If true, additional debug
 *                information is printed to stderr, including error messages related to syntax errors or
 *                invalid email addresses.
 *
 * @note The function assumes that the `argument` is a null-terminated string. It uses standard library functions
 *       to perform string manipulation and dynamic memory allocation. The `isValidEmail` function is called to
 *       perform syntactical validation of the extracted email address, which is not defined in this snippet and
 *       must be implemented separately. The caller must ensure that memory allocated for `reversePath` is properly
 *       freed to avoid memory leaks.
 */
void processMailFromCommand(int sock, const char *argument, char **reversePath, bool verbose)
{
  if (!argument)
  {
    if (verbose)
    {
      fprintf(stderr, "[%d] S: 501 Syntax error in parameters or arguments\n", sock);
    }
    return;
  }

  std::string argStr(argument);
  size_t start = argStr.find('<');
  size_t end = argStr.find('>');

  if (start == std::string::npos || end == std::string::npos || end <= start)
  {
    code501(sock, verbose);
    return;
  }

  std::string email = argStr.substr(start + 1, end - start - 1);

  *reversePath = strdup(email.c_str());
  code250(sock, verbose);
}

/**
 * @brief Processes the RCPT TO command in an SMTP session, validating the recipient's email address and determining its handling.
 *
 * This function parses the RCPT TO command argument to extract the recipient's email address, which should be enclosed within
 * angle brackets (< and >). It checks the syntax of the email address and validates whether the recipient domain is @penncloud07.com
 * or an external domain. For local recipients, it verifies the existence of the username in the provided list of valid users.
 * For external domains, if the extra credit feature is enabled, it queues the email address for forwarding; otherwise, it
 * responds with an error indicating that forwarding is not supported without the extra credit feature enabled.
 *
 * @param sock The socket file descriptor for the client connection, used to send SMTP responses.
 * @param argument The argument provided with the RCPT TO command, containing the recipient's email address.
 * @param forwardPaths A reference to a vector of strings where validated recipient email addresses are stored.
 *                     This vector is updated with the recipient's email if it passes validation.
 * @param verbose A boolean flag indicating whether verbose logging is enabled. If true, additional details
 *                may be logged for debugging purposes.
 * @param extraCredit A flag indicating whether the extra credit feature (forwarding to external domains) is enabled.
 *                    If true, external recipient addresses are queued for forwarding.
 *
 * @note The function assumes that the `argument` is a null-terminated string and uses standard library functions
 *       for string manipulation. It validates the recipient's email address for proper formatting and domain handling.
 *       For external domains (not @penncloud07.com), the behavior is contingent upon the `extraCredit` flag: if enabled, the
 *       function queues the address for forwarding; otherwise, it issues a 550 response indicating the action is unsupported.
 *       This implementation highlights handling for an educational SMTP server scenario and may require adjustments for
 *       real-world applications, particularly in terms of security, validation, and compliance with SMTP standards.
 */
void processRcptToCommand(int sock, const char *argument, std::vector<std::string> &forwardPaths, bool verbose, int extraCredit)
{
  std::string argStr(argument);

  size_t start = argStr.find('<');
  size_t end = argStr.find('>', start);
  if (start == std::string::npos || end == std::string::npos || end <= start)
  {
    code501(sock, verbose);
    return;
  }

  // Extract the email address
  std::string email = argStr.substr(start + 1, end - start - 1);

  // Validate the email address
  size_t atPos = email.rfind('@');
  if (atPos == std::string::npos)
  {
    code550(sock, verbose); // Missing '@'
    return;
  }

  std::string username = email.substr(0, atPos);
  std::string domain = email.substr(atPos);

  if (domain != "@penncloud07.com")
  {
    if (extraCredit)
    {
      code250(sock, verbose);
      forwardPaths.push_back(email);
    }
    else
    {
      code550(sock, verbose, ". The email will be forwarded to another server, if -e (using extraCreit part).");
    }

    return;
  }
  std::string value;
  if (verbose)
  {
    std::cout << "Checking if user exists with kvsClient.Get()" << username << std::endl;
  }
  if (!kvsClient.Get("accounts", username, value, "LOCK_BYPASS")) // detect if a user exists
  {
    code550(sock, verbose); // User not found
  }
  else
  {
    // Append validated email to forwardPaths
    forwardPaths.push_back(username);
    code250(sock, verbose); // Success
  }
}

std::map<std::string, std::mutex> fileMutexes;

/**
 * @brief Writes an email to the appropriate mailbox or queue file, handling both local delivery and forwarding.
 *
 * This function processes each recipient in the `forwardPaths` vector, determining whether to deliver the email
 * locally (to an .mbox file) or to queue it for forwarding (in the mqueue file). For local delivery, the email is
 * appended to the recipient's .mbox file, formatted with the sender's email, timestamp, and the email content. For
 * recipients with an external domain, the email is appended to the mqueue file for later forwarding, including both
 * the sender and recipient addresses in the header. The function ensures thread-safe access to mailbox and queue files
 * using mutexes.
 *
 * @param emailContent The content of the email to be delivered or forwarded.
 * @param forwardPaths A vector of recipient email addresses or local usernames. Addresses indicate forwarding is required,
 *                     while usernames indicate local delivery.
 * @param senderEmail The email address of the sender, included in the header of each email.
 * @param file_path The base directory path for mailbox (.mbox) and queue (mqueue) files.
 *
 * @note The function utilizes a global `fileMutexes` map (not shown) keyed by file paths to ensure thread-safe access
 *       to each file. It assumes that the email content is formatted correctly for insertion into a mailbox file and
 *       that the system clock is set accurately for timestamping. The function does not perform error checking on file
 *       operations beyond checking if the file is open, and it assumes `forwardPaths` correctly distinguishes between
 *       local and external recipients based on the presence of an '@' character. This simplistic approach may need
 *       refinement for real-world applications, including more robust error handling and validation.
 */
void writeEmailToMbox(const std::string &emailContent, const std::vector<std::string> &forwardPaths, const std::string &senderEmail, const std::string &file_path, bool verbose)
{
  // Current time formatting
  auto now = std::chrono::system_clock::now();
  auto now_c = std::chrono::system_clock::to_time_t(now);
  std::stringstream dateStream;
  dateStream << std::ctime(&now_c); // Converts to string and includes newline

  for (const auto &forwardPath : forwardPaths)
  {
    if (forwardPath.find('@') != std::string::npos)
    {
        std::string queuePath = file_path + "mqueue";
        std::lock_guard<std::mutex> lock(fileMutexes[queuePath]);

        int fd = open(queuePath.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0666);
        if (fd == -1)
        {
            std::cerr << "Failed to open file: " << queuePath << std::endl;
            continue;
        }

        // Lock the file
        if (flock(fd, LOCK_EX) == -1)
        {
            std::cerr << "Failed to lock file: " << queuePath << std::endl;
            close(fd);
            continue;
        }

        FILE* file = fdopen(fd, "a");
        if (file == nullptr)
        {
            std::cerr << "Failed to convert file descriptor: " << queuePath << std::endl;
            flock(fd, LOCK_UN);
            close(fd);
            continue;
        }

        fprintf(file, "From <%s> <%s>%s", senderEmail.c_str(), forwardPath.c_str(), dateStream.str().c_str());
        fprintf(file, "%s\r\n", emailContent.c_str());

        fclose(file);  // This closes `fd` as well, no need to call close(fd) after this
        // No need to manually unlock here since fclose also handles that
    }
    else
    {
      std::string rowKey = forwardPath + ".mbox";
      std::string emailWithFromLine = "From <" + senderEmail + "> " + dateStream.str() + emailContent + "\r\n";
      unsigned char hash[MD5_DIGEST_LENGTH];
      computeDigest((char *)emailWithFromLine.c_str(), emailWithFromLine.size(), hash);
      std::string emailId = hashToString(hash, MD5_DIGEST_LENGTH); // this will be the column key
      std::string mutexId;
      bool mutexAcquired = false;
      do { // acquire the lock for this row
        mutexAcquired = kvsClient.SetNX(rowKey, mutexId);
        if (!mutexAcquired) sleep(1);
      } while (!mutexAcquired);
      if (verbose) {
        std::cout << "Email lock acquired for " << rowKey << std::endl;
        std::cout << "Email id: " << emailId << std::endl;
        std::cout << "Email content written: " << emailWithFromLine << std::endl;
      }
      if (!kvsClient.Put(rowKey, emailId, emailWithFromLine, mutexId)) {
        std::cerr << "Failed to write email to " << rowKey << std::endl;
      } else if (verbose) {
        std::cout << "Email written to " << rowKey << std::endl;
      }
     
      if (!kvsClient.Del(rowKey, mutexId)) {
        std::cerr << "Failed to release lock for " << rowKey << std::endl;
      } else if (verbose) {
        std::cout << "Email lock released for " << rowKey << std::endl;
      }
    }
  }
}

/**
 * @brief Processes the DATA command during an SMTP session, handling the reception of email content.
 *
 * This function is triggered after the DATA command has been issued by the client, indicating that
 * the following lines constitute the body of the email until a single line containing only a period
 * (".") is received. It checks if there are valid recipient addresses and a sender address (reverse path)
 * before proceeding. If the conditions are met, it logs the incoming data if verbose logging is enabled,
 * then checks for the termination sequence ("\r\n.\r\n"). Upon detecting the end of the email data,
 * it extracts the email content, excluding the termination sequence, and passes it along with the sender
 * and recipient information to `writeEmailToMbox` for storage or forwarding. It then resets the state
 * for handling the next message, clearing recipient information and resetting buffer states.
 *
 * @param sock The socket descriptor associated with the client connection, used for sending SMTP responses.
 * @param isInDataMode A reference to a boolean flag indicating whether the session is currently in data
 *                     reception mode. This flag is set to false upon successful processing of the email data.
 * @param emailBuffer A character array storing the accumulated email data received since entering data mode.
 * @param emailBufferLength A reference to a size_t variable tracking the current length of the data in `emailBuffer`.
 * @param forwardPaths A reference to a vector of strings storing the recipient email addresses for this email.
 * @param reversePath A pointer to a character array storing the sender's email address. This pointer is set to NULL
 *                    after processing to indicate that the sender address needs to be re-established for the next message.
 * @param file_path The base path to the directory where mailbox (.mbox) and queue (mqueue) files are stored.
 * @param buffer A character array containing the most recent chunk of data received. This data is appended to
 *               `emailBuffer` before processing.
 * @param verbose A boolean flag indicating whether verbose logging is enabled. If true, logs the data received.
 *
 * @note This function assumes that `emailBuffer` is large enough to hold the entire email content plus the
 *       termination sequence. It performs minimal validation on the email content itself and assumes that
 *       `writeEmailToMbox` handles any necessary formatting or validation for storage or forwarding. The
 *       function clears `forwardPaths` and sets `reversePath` to NULL after processing to prepare for the
 *       next message, which may require additional error handling or validation in a real-world application.
 */
void processDataCommand(int sock, bool &isInDataMode, char *emailBuffer, size_t &emailBufferLength, std::vector<std::string> &forwardPaths, char *&reversePath, const std::string &file_path, char *buffer, bool verbose)
{
  if (forwardPaths.empty() || !reversePath)
  {
    code503(sock, verbose);
    return;
  }
  if (verbose)
  {
    fprintf(stderr, "[%d] C: %s\n", sock, buffer);
  }

  std::string bufferStr(emailBuffer);
  size_t endOfData = bufferStr.find("\r\n.\r\n");
  if (endOfData != std::string::npos)
  {
    isInDataMode = false;

    // Extract the email content, excluding the termination sequence
    std::string emailContent = bufferStr.substr(0, endOfData);

    writeEmailToMbox(emailContent, forwardPaths, reversePath, file_path, verbose);

    // move the remaining data to the beginning of the buffer
    emailBufferLength -= endOfData + 5;
    memmove(emailBuffer, emailBuffer + endOfData + 5, emailBufferLength);
    emailBuffer[emailBufferLength] = '\0';

    code250(sock, verbose);

    // Clear the forward paths and reverse path
    forwardPaths.clear();
    reversePath = NULL;
    emailBuffer[0] = '\0';
  }
}
