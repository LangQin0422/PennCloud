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
 * Sends a 220 response code to the client indicating that the service is ready.
 *
 * @param sock The socket descriptor for the client connection.
 * @param verbose A flag indicating whether to print verbose output.
 */
void code220(int sock, bool verbose)
{
  char *greeting = "220 penncloud07.com Service ready\r\n";
  write(sock, greeting, strlen(greeting));
  if (verbose)
  {
    fprintf(stderr, "[%d] S: %s", sock, greeting);
  }
}

/**
 * Sends a 221 response code to the client indicating that the service is closing the transmission channel.
 *
 * @param sock The socket file descriptor.
 * @param verbose A boolean value indicating whether to print verbose output.
 */
void code221(int sock, bool verbose)
{
  char *closing = "221 penncloud07.com Service closing transmission channel\r\n";
  write(sock, closing, strlen(closing));
  if (verbose)
  {
    fprintf(stderr, "[%d] S: %s", sock, closing);
    fprintf(stderr, "[%d] Connection closed\n", sock);
  }
}

/**
 * Sends a 250 OK response to the specified socket.
 *
 * @param sock The socket to send the response to.
 * @param verbose If true, prints the response to stderr.
 */
void code250(int sock, bool verbose)
{
  char *ok = "250 OK\r\n";
  write(sock, ok, strlen(ok));
  if (verbose)
  {
    fprintf(stderr, "[%d] S: %s", sock, ok);
  }
}

/**
 * Sends a 250 response code to the specified socket with an optional message.
 *
 * @param sock The socket to send the response to.
 * @param verbose If true, prints the response to stderr.
 * @param message The optional message to include in the response.
 */
void code250(int sock, bool verbose, const char *message)
{
  char ok[256];
  strcpy(ok, "250 ");
  strcat(ok, message);
  strcat(ok, "\r\n");
  write(sock, ok, strlen(ok));
  if (verbose)
  {
    fprintf(stderr, "[%d] S: %s", sock, ok);
  }
}

/**
 * Sends the code 354 to the specified socket and provides a verbose output if requested.
 *
 * @param sock The socket to send the code to.
 * @param verbose Whether to provide a verbose output or not.
 */
void code354(int sock, bool verbose)
{
  char *start_input = "354 Start mail input; end with <CRLF>.<CRLF>\r\n";
  write(sock, start_input, strlen(start_input));
  if (verbose)
  {
    fprintf(stderr, "[%d] S: %s", sock, start_input);
  }
}

/**
 * Sends a 421 response to the client indicating that the service is not available and closes the transmission channel.
 *
 * @param sock The socket file descriptor.
 * @param verbose A flag indicating whether to print verbose output.
 */
void code421(int sock, bool verbose)
{
  char *service_unavailable = "421 Service not available, closing transmission channel\r\n";
  write(sock, service_unavailable, strlen(service_unavailable));
  if (verbose)
  {
    fprintf(stderr, "[%d] S: %s", sock, service_unavailable);
  }
}

/**
 * Sends a 500 response to the client indicating a syntax error.
 *
 * @param sock The socket file descriptor.
 * @param verbose If true, prints the response to stderr.
 */
void code500(int sock, bool verbose)
{
  char *syntax_error = "500 Syntax error, command unrecognized\r\n";
  write(sock, syntax_error, strlen(syntax_error));
  if (verbose)
  {
    fprintf(stderr, "[%d] S: %s", sock, syntax_error);
  }
}

/**
 * Sends a 500 Syntax error response to the specified socket with an optional message.
 *
 * @param sock The socket to send the response to.
 * @param verbose If true, prints the response to stderr.
 * @param message An optional message to include in the response.
 */
void code500(int sock, bool verbose, const char *message)
{
  char syntax_error[256];
  strcpy(syntax_error, "500 Syntax error");
  strcat(syntax_error, message);
  strcat(syntax_error, "\r\n");
  write(sock, syntax_error, strlen(syntax_error));
  if (verbose)
  {
    fprintf(stderr, "[%d] S: %s", sock, syntax_error);
  }
}

/**
 * Sends a 501 response to the client indicating a syntax error in parameters or arguments.
 *
 * @param sock The socket descriptor to write the response to.
 * @param verbose If true, prints the response to stderr.
 */
void code501(int sock, bool verbose)
{
  char *syntax_error = "501 Syntax error in parameters or arguments\r\n";
  write(sock, syntax_error, strlen(syntax_error));
  if (verbose)
  {
    fprintf(stderr, "[%d] S: %s", sock, syntax_error);
  }
}

/**
 * Sends a 501 Syntax error response to the client.
 *
 * @param sock The socket descriptor.
 * @param verbose Flag indicating whether to print verbose output.
 * @param message The error message to include in the response.
 */
void code501(int sock, bool verbose, const char *message)
{
  char syntax_error[256];
  strcpy(syntax_error, "501 Syntax error");
  strcat(syntax_error, message);
  strcat(syntax_error, "\r\n");
  write(sock, syntax_error, strlen(syntax_error));
  if (verbose)
  {
    fprintf(stderr, "[%d] S: %s", sock, syntax_error);
  }
}

/**
 * Sends a 503 response to the client indicating a bad sequence of commands.
 *
 * @param sock The socket file descriptor.
 * @param verbose A flag indicating whether to print verbose output.
 */
void code503(int sock, bool verbose)
{
  char *bad_sequence = "503 Bad sequence of commands\r\n";
  write(sock, bad_sequence, strlen(bad_sequence));
  if (verbose)
  {
    fprintf(stderr, "[%d] S: %s", sock, bad_sequence);
  }
}

/**
 * Sends a 503 response to the client indicating a bad sequence of commands.
 *
 * @param sock The socket file descriptor.
 * @param verbose Flag indicating whether to print verbose output.
 * @param message The error message to include in the response.
 */
void code503(int sock, bool verbose, const char *message)
{
  char bad_sequence[256];
  strcpy(bad_sequence, "503 Bad sequence of commands ");
  strcat(bad_sequence, message);
  strcat(bad_sequence, "\r\n");
  write(sock, bad_sequence, strlen(bad_sequence));
  if (verbose)
  {
    fprintf(stderr, "[%d] S: %s", sock, bad_sequence);
  }
}

/**
 * Sends a 550 response code to the client indicating that the requested action
 * was not taken due to mailbox unavailability.
 *
 * @param sock The socket file descriptor.
 * @param verbose A flag indicating whether to print verbose output.
 */
void code550(int sock, bool verbose)
{
  char *mailbox_unavailable = "550 Requested action not taken: mailbox unavailable\r\n";
  write(sock, mailbox_unavailable, strlen(mailbox_unavailable));
  if (verbose)
  {
    fprintf(stderr, "[%d] S: %s", sock, mailbox_unavailable);
  }
}

/**
 * Sends a 550 response code to the specified socket with the given message.
 * If verbose is true, also prints the response to stderr.
 *
 * @param sock The socket to send the response to.
 * @param verbose Whether to print the response to stderr.
 * @param message The message to include in the response.
 */
void code550(int sock, bool verbose, const char *message)
{
  char mailbox_unavailable[256];
  strcpy(mailbox_unavailable, "550 Requested action not taken: mailbox unavailable ");
  strcat(mailbox_unavailable, message);
  strcat(mailbox_unavailable, "\r\n");
  write(sock, mailbox_unavailable, strlen(mailbox_unavailable));
  if (verbose)
  {
    fprintf(stderr, "[%d] S: %s", sock, mailbox_unavailable);
  }
}
