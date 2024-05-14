#include <resolv.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#include <tuple>
#include <sys/file.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <set>
#include <cstdio>

#include <io_helper.hh>

/**
 * @brief Extracts and returns the domain part from a given email address.
 *
 * This function identifies the position of the '@' character in the email address and extracts
 * the substring that follows it, which corresponds to the domain part of the email. If the '@'
 * character is not found, indicating that the input may not be a valid email address, the function
 * returns an empty string.
 *
 * @param email The email address from which to extract the domain part.
 * @return std::string The domain part of the email address if the '@' character is present. If the '@'
 *                     character is absent or if the email address is otherwise malformed, returns an empty string.
 *
 * @note This function assumes the email address is in a standard format (local-part@domain) and does not
 *       perform comprehensive validation of the email address structure. It simply extracts the substring
 *       after the '@' character.
 */
std::string extractDomain(const std::string &email)
{
  size_t atPos = email.find('@');
  if (atPos != std::string::npos)
  {
    return email.substr(atPos + 1);
  }
  return "";
}

/**
 * @brief Performs a DNS query to look up the Mail Exchange (MX) record for a given domain. This function queries
 *        the DNS for MX records associated with the specified domain to determine the mail server responsible for
 *        receiving email on behalf of the domain. It returns the hostname of the mail server with the highest priority
 *        (lowest preference value) found in the DNS MX records. If the query fails or no MX records are found, an
 *        empty string is returned.
 *
 * @param domain The domain name for which to find the MX record. This is typically the part of an email address
 *               after the '@' symbol.
 * @param verbose A flag indicating whether to output verbose logging information. If set to a non-zero value,
 *                additional details about the DNS query process may be printed to stderr, particularly if the query fails.
 *
 * @return std::string The hostname of the primary mail server (MX record) for the specified domain. If the DNS query
 *                     fails, if no MX records are found, or if any errors occur during the query process, an empty
 *                     string is returned. The function returns the hostname of the first MX record found, which is
 *                     assumed to be the one with the highest priority.
 *
 * @note This function uses the `res_query` function from the `libresolv` library to perform the DNS query, and
 *       `ns_parserr` and `dn_expand` functions to parse the query result. The function is designed to return the
 *       hostname of the first MX record found, without specifically sorting by preference values. In practice, MX
 *       records with lower preference values should be attempted first when sending email. Implementers may wish
 *       to extend this function to sort by preference or to handle multiple MX records more robustly.
 */
std::string lookupMailServer(const std::string &domain, int verbose)
{
  unsigned char queryResult[NS_PACKETSZ]; // Buffer to store the query result
  int queryLen = res_query(domain.c_str(), C_IN, ns_t_mx, queryResult, sizeof(queryResult));

  if (queryLen < 0)
  {
    if (verbose)
      fprintf(stderr, "S: DNS query failed for domain: %s", domain.c_str());
    return "";
  }

  ns_msg handle;
  ns_initparse(queryResult, queryLen, &handle);
  int msgCount = ns_msg_count(handle, ns_s_an);

  ns_rr record;
  for (int i = 0; i < msgCount; i++)
  {
    if (ns_parserr(&handle, ns_s_an, i, &record) == 0)
    {
      auto *rdata = ns_rr_rdata(record);
      // The MX record data starts with a 16-bit preference value, followed by the mail server name.
      int preference = ntohs(*(uint16_t *)rdata);

      // The mail server name is encoded as a DNS label; use dn_expand to extract it.
      char mxHost[NS_MAXDNAME];
      dn_expand(ns_msg_base(handle), ns_msg_end(handle), rdata + 2, mxHost, sizeof(mxHost));

      return std::string(mxHost); // Return the first MX host found
    }
  }

  return "";
}

/**
 * @brief Establishes a TCP connection to a specified mail server on the SMTP port (25).
 *
 * This function attempts to create a socket and connect to the given server address on port 25,
 * which is the standard port for SMTP communication. It utilizes DNS resolution to convert the
 * server address from a domain name to an IP address. If any step of the process fails, such as
 * socket creation, DNS resolution, or establishing a connection, the function prints an error message
 * if verbose logging is enabled and returns -1 to indicate failure.
 *
 * @param serverAddress The domain name or IP address of the mail server to connect to.
 * @param verbose A flag indicating whether to output verbose logging information. If set to a non-zero value,
 *                error messages are printed to stderr for troubleshooting purposes.
 *
 * @return int A socket file descriptor for the established connection if successful; -1 otherwise.
 *
 * @note The function uses the older `gethostbyname` function for DNS resolution, which may not be suitable
 *       for environments where IPv6 is in use or preferred. Consider using `getaddrinfo` for a more modern
 *       and flexible approach to DNS resolution and socket setup, especially to ensure compatibility with
 *       both IPv4 and IPv6.
 */
int connectToMailServer(const std::string &serverAddress, int verbose)
{
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
  {
    if (verbose)
      fprintf(stderr, "S: Failed to create socket");
    return -1;
  }

  struct hostent *server = gethostbyname(serverAddress.c_str());
  if (server == nullptr)
  {
    if (verbose)
      fprintf(stderr, "S: No such host: %s", serverAddress.c_str());
    return -1;
  }

  struct sockaddr_in serv_addr;
  std::memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  std::memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
  serv_addr.sin_port = htons(25); // SMTP port

  if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
  {
    if (verbose)
      fprintf(stderr, "S: Failed to connect to the mail server");
    return -1;
  }

  return sock;
}

/**
 * @brief Sends an SMTP command to the server and checks the response against an expected response code.
 *
 * This function sends a specified SMTP command to the mail server and waits for a response. It logs the command and
 * the server's response if verbose logging is enabled. The function then checks if the beginning of the server's response
 * matches the expected response code. If no specific response is expected (indicated by an empty expectedResponse string),
 * the function assumes success. The function primarily checks for error response codes starting with '4' (temporary failure)
 * or '5' (permanent failure) and treats any other response as success unless a specific response is expected.
 *
 * @param sock The socket file descriptor through which the command is sent and the response is received.
 * @param command The SMTP command to be sent to the server, formatted as a string.
 * @param expectedResponse The expected response code as a string. This function checks if the response from the server
 *                         starts with this code. If this parameter is empty, the function does not check the response code
 *                         and assumes success.
 * @param verbose A flag indicating whether to output verbose logging information. If set to a non-zero value,
 *                command and response messages are printed to stderr for troubleshooting purposes.
 *
 * @return bool Returns true if the server's response indicates success (does not start with '4' or '5', or matches the
 *              expectedResponse if provided). Returns false if the server's response indicates an error (starts with '4' or '5').
 *
 * @note This implementation simplifies response handling by focusing on the first character of the response code.
 *       More sophisticated handling may require parsing the full response code and message for precise control
 *       over different SMTP scenarios. Additionally, the function currently lacks error handling for network issues;
 *       enhancements could include checking the return values of `send` and `recv` to ensure proper operation.
 */
bool sendSMTPCommand(int sock, const std::string &command, const std::string &expectedResponse, int verbose)
{
  if (verbose)
  {
    fprintf(stderr, "[%d] C: %s", sock, command.c_str());
  }
  send(sock, command.c_str(), command.length(), 0);

  char buffer[1024] = {0};
  recv(sock, buffer, sizeof(buffer) - 1, 0);

  if (verbose)
  {
    fprintf(stderr, "[%d] S: %s", sock, buffer);
  }

  if (expectedResponse.empty())
  {
    return true; // No specific response expected
  }
  // extract the response code
  std::string responseCode = std::string(buffer).substr(0, 1);

  return std::strncmp(responseCode.c_str(), "4", expectedResponse.length()) != 0 && std::strncmp(responseCode.c_str(), "5", expectedResponse.length()) != 0;
}

/**
 * @brief Sends an email using SMTP protocol commands over an established socket connection.
 *
 * This function orchestrates the sending of an email by issuing a series of SMTP commands to the server
 * through a socket connection. It starts with the HELO command to greet the server and proceeds through
 * specifying the sender and recipient(s) with MAIL FROM and RCPT TO, respectively. The DATA command
 * initiates the transmission of the email content, which is terminated with a single period on a line by itself.
 * Finally, the QUIT command ends the SMTP session. Each command's expected response begins with '2', indicating
 * successful processing by the server, except for QUIT which does not necessarily require a specific response.
 * If any command fails to elicit the expected response, the function aborts the email transmission process and returns false.
 *
 * @param sock The socket file descriptor for the connection to the SMTP server.
 * @param from The email address of the sender, used in the MAIL FROM command.
 * @param to The email address of the recipient, used in the RCPT TO command.
 * @param emailContent The full content of the email to be sent, inserted between the DATA command and its terminating sequence.
 * @param verbose A flag indicating whether detailed command and response information should be logged for troubleshooting.
 *
 * @return bool Returns true if all SMTP commands are successfully executed and the expected responses are received;
 *              otherwise, returns false if any command fails, halting the email transmission process.
 *
 * @note The function utilizes a vector of tuples to manage the sequence of SMTP commands and their expected response
 *       codes for streamlined processing. It leverages the sendSMTPCommand function, which abstracts the details of
 *       sending each command and evaluating the server's response, enhancing code readability and maintainability.
 */
bool sendEmail(int sock, const std::string &from, const std::string &to, const std::string &emailContent, int verbose)
{
  // Prepare the sequence of commands and expected responses
  std::vector<std::tuple<std::string, std::string>> commands = {
      {"HELO penncloud07.com\r\n", "2"},
      {"MAIL FROM:<" + from + ">\r\n", "2"},
      {"RCPT TO:<" + to + ">\r\n", "2"},
      {"DATA\r\n", "2"},
      {emailContent + "\r\n.\r\n", "2"},
      {"QUIT\r\n", ""}};

  // Iterate through each command and send it

  for (size_t i = 0; i < commands.size(); i++)
  {
    const std::string &command = std::get<0>(commands[i]);
    const std::string &expectedResponse = std::get<1>(commands[i]);

    if (!sendSMTPCommand(sock, command, expectedResponse, verbose))
    {
      return false; // Stop and return false if any command fails
    }
  }

  // All commands were successful
  return true;
}

/**
 * @brief Reads the content of the 'mqueue' file specified by the file descriptor and extracts individual messages.
 *
 * This function reads the entire content of a file (typically named 'mqueue') designated by the provided file descriptor `fd`.
 * It accumulates the content into a single string and then utilizes the `splitMessages` function to divide this content into
 * individual email messages. Each message is expected to be delimited in a manner consistent with the `splitMessages` function's
 * expectations (e.g., by specific separators or patterns). The extracted messages are stored in the provided vector `messages`.
 * This function is useful for processing queued email messages stored in a file, enabling subsequent operations such as sending
 * these messages to their intended recipients.
 *
 * @param fd The file descriptor of the 'mqueue' file from which to read the email messages. This file descriptor should be open
 *           and ready for reading.
 * @param messages A reference to a vector of strings. This vector will be populated with the individual messages extracted from
 *                 the 'mqueue' file's content. The vector is cleared and replaced with the new set of messages extracted during
 *                 the function's execution.
 *
 * @note The function assumes that `fd` is a valid, open file descriptor pointing to a readable file. It does not handle opening
 *       or closing the file descriptor, nor does it manage error conditions related to file I/O operations beyond the basic read.
 *       The caller is responsible for managing the file descriptor's lifecycle and for ensuring that `splitMessages` is compatible
 *       with the format of the queued messages in the 'mqueue' file.
 */
void extract_mqueue(const int &fd, std::vector<std::string> &messages)
{
  std::string mboxContent = "";
  char buffer[4096];
  ssize_t bytesRead;

  while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0)
  {
    mboxContent.append(buffer, bytesRead);
  }

  messages = splitMessages(mboxContent, true);
}
