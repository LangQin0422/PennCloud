#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <getopt.h>
#include <errno.h>

/**
 * @brief Sends an error response to the client over the specified socket. This function constructs an error message
 *        prefixed with "-ERR", followed by the provided message, and sends it to the client using the specified socket.
 *        Additionally, if verbose logging is enabled, the error response is also logged to stderr with the socket descriptor
 *        and the sent message for debugging purposes.
 *
 * @param sock The socket descriptor used for communication with the client. The constructed error message is sent through this socket.
 * @param verbose A boolean flag indicating whether to log the error message to stderr. If true, the message is logged for debugging.
 * @param message A C-string containing the error message to be sent to the client. This message is appended to the "-ERR" prefix.
 *
 * @return void The function does not return a value. It sends the constructed error response directly to the client and optionally logs it.
 */
void err_code(int sock, bool verbose, const char *message)
{
  char response[8192];
  sprintf(response, "-ERR %s\r\n", message);
  write(sock, response, strlen(response));
  if (verbose)
  {
    fprintf(stderr, "[%d] S: %s", sock, response);
  }
}

/**
 * @brief Sends a positive acknowledgment response to the client over the specified socket. This function constructs a success message
 *        prefixed with "+OK", followed by the provided message, and sends it to the client using the specified socket.
 *        If verbose logging is enabled, the success response is also logged to stderr with the socket descriptor and the sent message
 *        for auditing or debugging purposes.
 *
 * @param sock The socket descriptor used for communication with the client. The constructed success message is sent through this socket.
 * @param verbose A boolean flag indicating whether to log the success message to stderr. If true, the message is logged, providing visibility
 *                into successful operations.
 * @param message A C-string containing the success message to be sent to the client. This message is appended to the "+OK" prefix, forming
 *                the complete response.
 *
 * @return void The function does not return a value. It sends the constructed positive acknowledgment response directly to the client and
 *              optionally logs it.
 */
void ok_code(int sock, bool verbose, const char *message)
{
  char response[8192];
  sprintf(response, "+OK %s\r\n", message);
  write(sock, response, strlen(response));
  if (verbose)
  {
    fprintf(stderr, "[%d] S: %s", sock, response);
  }
}

/**
 * @brief Sends a message to the client over the specified socket without any specific prefix. This function is used for sending
 *        data or responses that do not require the standard "+OK" or "-ERR" prefixes, such as the body of an email message or
 *        multi-line responses in POP3. The message is sent exactly as provided. If verbose logging is enabled, the message is
 *        also logged to stderr with the socket descriptor for debugging or auditing purposes.
 *
 * @param sock The socket descriptor used for communication with the client. The message is sent through this socket.
 * @param verbose A boolean flag indicating whether to log the message to stderr. If true, the message, along with the socket descriptor,
 *                is logged, allowing for easier debugging and monitoring of the server-client communication.
 * @param message A C-string containing the message to be sent to the client. This message is sent as-is, without any modifications or
 *                additional formatting.
 *
 * @return void The function does not return a value. It directly sends the provided message to the client and optionally logs it.
 */
void msg_code(int sock, bool verbose, const char *message)
{
  char response[8192];
  sprintf(response, "%s", message);
  write(sock, response, strlen(response));
  if (verbose)
  {
    fprintf(stderr, "[%d] S: %s", sock, response);
  }
}
