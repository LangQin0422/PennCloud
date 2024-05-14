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
#include <dirent.h>
#include <iostream>
#include <vector>
#include <algorithm>

#include "../include/smpt_msg.hh"
#include "../include/io_helper.hh"
#include "../include/smpt_command.hh"
#include "../include/echoserver.hh"
#include "webmail.hh"

#define DEFAULT_PORT 2500
#define MAX_CONNECTIONS 100
#define BUFFER_SIZE 2048

int verbose = 0;                     // Verbose output
int server_fd;                       // Server socket file descriptor
int client_sockets[MAX_CONNECTIONS]; // Active client sockets
int extraCredit = 0;                 // Extra credit flag
pthread_mutex_t client_sockets_mutex = PTHREAD_MUTEX_INITIALIZER;

struct ThreadArgs
{
  int *client_socket;
  std::string file_path;
};

KVSClient kvsClient;
Config config;

void testSimple()
{
  std::cout << "Testing simple put and get..." << std::endl;
  std::string value;

  kvsClient.Put("row12", "col12", "value1", "LOCK_BYPASS");
  assert(kvsClient.Get("row12", "col12", value, "LOCK_BYPASS"));
  assert(value == "value1");

  kvsClient.Delete("row12", "col12", "LOCK_BYPASS");
  assert(!kvsClient.Get("row12", "col12", value, "LOCK_BYPASS"));

  std::cout << "Simple put and get test passed!" << std::endl;
}

void initKVS()
{
  const bool localKVS = false;
  std::vector<std::vector<std::string>> clusters;

  if (localKVS)
  {
    clusters = {
      {"127.0.0.1:50051", "127.0.0.1:50052", "127.0.0.1:50053"}};
  } else {
    clusters = {
          {"34.171.122.180:50051", "34.171.122.180:50052", "34.171.122.180:50053"},
          {"34.70.254.14:50051", "34.70.254.14:50052", "34.70.254.14:50053"}};
  }

  kvsClient = KVSClient(clusters);
}

int main(int argc, char *argv[])
{
  initKVS();
  struct sockaddr_in address;
  int addrlen = sizeof(address);
  int port = DEFAULT_PORT;
  int option;
  char *file_path = NULL;

  // Initialize client sockets array
  memset(client_sockets, 0, sizeof(client_sockets));

  // Parse command-line options
  while ((option = getopt(argc, argv, "p:ave")) != -1)
  {
    switch (option)
    {
    case 'p':
      port = atoi(optarg);
      break;
    case 'a':
      fprintf(stderr, "Author: Zhengyi Xiao / zxiao98\n");
      exit(EXIT_SUCCESS);
    case 'v':
      verbose = 1;
      break;
    case 'e':
      extraCredit = 1;
      break;
    default:
      fprintf(stderr, "Usage: %s [-p port] [-a] [-v] [e] <file_path>\n", argv[0]);
      exit(EXIT_FAILURE);
    }
  }
  if (optind < argc)
  {
    file_path = argv[optind];
  }
  else
  {
    fprintf(stderr, "Expected file path after options\n");
    exit(EXIT_FAILURE);
  }

  // Register signal handler for SIGINT
  signal(SIGINT, signal_handler);

  // Creating socket file descriptor
  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
  {
    perror("Socket failed");
    exit(EXIT_FAILURE);
  }

  /* check reusing port */
  int opt = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0)
  {
    fprintf(stderr, "Error setting socket options.\n");
    return 1;
  }

  // Forcefully attaching socket to the port
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
  {
    perror("Bind failed");
    exit(EXIT_FAILURE);
  }
  if (listen(server_fd, MAX_CONNECTIONS) < 0)
  {
    perror("Listen");
    exit(EXIT_FAILURE);
  }

  if (verbose)
  {
    fprintf(stderr, "Server started on port %d\n", port);
  }

  while (1)
  {
    int new_socket;
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
    {
      if (errno == EINTR)
      {
        // Interrupted by signal handler
        break;
      }
      perror("Accept");
      continue;
    }

    if (verbose)
    {
      fprintf(stderr, "[%d] New connection\n", new_socket);
    }

    // Add new socket to the list of client sockets
    pthread_mutex_lock(&client_sockets_mutex);
    for (int i = 0; i < MAX_CONNECTIONS; i++)
    {
      if (client_sockets[i] == 0)
      {
        client_sockets[i] = new_socket;
        break;
      }
    }
    pthread_mutex_unlock(&client_sockets_mutex);

    pthread_t thread_id;
    int *new_sock = (int *)malloc(sizeof(int));
    *new_sock = new_socket;

    ThreadArgs *args = new ThreadArgs;
    args->client_socket = new_sock;
    args->file_path = file_path;

    // Creating a new thread for each connection
    if (pthread_create(&thread_id, nullptr, handle_client, (void *)args) != 0)
    {
      perror("pthread_create failed");
      code421(*(int *)new_sock, verbose);
      close(new_socket);
      delete args;
    }
    else
    {
      pthread_detach(thread_id); // Threads will be detached
    }
  }

  close_all_clients(); // Close all client connections gracefully
  close(server_fd);    // Finally, close the server socket
  return 0;
}

void *handle_client(void *args)
{
  ThreadArgs *threadArgs = static_cast<ThreadArgs *>(args);
  int *client_socket = threadArgs->client_socket;

  int sock = *(int *)client_socket;
  free(client_socket);

  char *dynamicBuffer = NULL; // Buffer to accumulate data
  size_t bufferLength = 0;    // Current length of the buffer

  bool isQuit = false;       // Flag to check if the client has sent a QUIT command
  bool isHello = false;      // Flag to check if the client has sent a HELO command
  bool isInDataMode = false; // Flag to track if we are in DATA input mode

  char buffer[BUFFER_SIZE]; // Temporary buffer to read incoming data
  int read_size;

  char *domain = NULL;
  char *reversePath = NULL;
  std::vector<std::string> forwardPaths;

  // Send the greeting message
  code220(sock, verbose);

  char emailBuffer[BUFFER_SIZE * 10]; // Buffer to store email content, adjust size as needed
  int emailBufferLength = 0;          // Length of current email content

  while ((read_size = recv(sock, buffer, BUFFER_SIZE - 1, 0)) > 0)
  {
    buffer[read_size] = '\0';

    dynamicBuffer = (char *)realloc(dynamicBuffer, bufferLength + read_size + 1);
    memcpy(dynamicBuffer + bufferLength, buffer, read_size + 1);
    bufferLength += read_size;

    if (isInDataMode) // swith to data mode for email content
    {
      processDataCommand(sock, isInDataMode, dynamicBuffer, bufferLength, forwardPaths, reversePath, threadArgs->file_path, buffer, verbose);
      continue;
    }

    char *crlf;
    while ((crlf = strstr(dynamicBuffer, "\r\n")) != NULL)
    {
      *crlf = '\0'; // Terminate the command string

      char *command = strtok(dynamicBuffer, " ");
      char *argument = strtok(NULL, "\r\n");

      if (verbose && command)
      {
        fprintf(stderr, "[%d] C: %s %s\n", sock, command, argument ? argument : "");
      }

      bool proceedWithCommand = true; // Flag to check if the command should be processed

      if (strcasecmp(command, "HELO") == 0)
      {
        try
        {
          if (reversePath) // if MAIL FROM has been sent, it is not in the initial state
          {
            code503(sock, verbose, "server is not in the initial state");
            isHello = false;
            proceedWithCommand = false;
          }
          if (proceedWithCommand)
          {
            code250(sock, verbose, "penncloud07.com");
            isHello = true;
            domain = argument;
            reversePath = NULL;
            forwardPaths.clear();
          }
        }
        catch (const std::exception &e)
        {
          code501(sock, verbose);
          isHello = false; // stay in the same state.
        }
      }
      else if (strcasecmp(command, "MAIL") == 0)
      {
        while (argument && *argument == ' ')
          argument++;

        if (argument == NULL)
        {
          code500(sock, verbose);
          proceedWithCommand = false;
        }

        if (proceedWithCommand && strncasecmp(argument, "FROM:", 5) != 0)
        {
          code500(sock, verbose);
          proceedWithCommand = false;
        }
        if (proceedWithCommand && !isHello) // HELO command must be sent before MAIL FROM
        {
          code501(sock, verbose);
          proceedWithCommand = false;
        }
        if (proceedWithCommand && reversePath) // Only one MAIL FROM is allowed, so replace the old one
        {
          code503(sock, verbose, "Sender already specified, the old one will be replaced");
        }

        if (proceedWithCommand)
          processMailFromCommand(sock, argument, &reversePath, verbose);
      }
      else if (strcasecmp(command, "RCPT") == 0)
      {
        while (argument && *argument == ' ')
          argument++;
        if (argument == NULL)
        {
          code500(sock, verbose);
          proceedWithCommand = false;
        }
        if (proceedWithCommand && strncasecmp(argument, "TO:", 3) != 0)
        {
          code500(sock, verbose);
          proceedWithCommand = false;
        }
        if (proceedWithCommand && !isHello) // HELO command must be sent before MAIL FROM
        {
          code501(sock, verbose);
          proceedWithCommand = false;
        }
        if (proceedWithCommand && !reversePath) // MAIL FROM must be sent before RCPT TO
        {
          code503(sock, verbose);
          proceedWithCommand = false;
        }

        if (proceedWithCommand)
          processRcptToCommand(sock, argument, forwardPaths, verbose, extraCredit);
      }
      else if (strcasecmp(command, "DATA") == 0)
      {
        if (!isHello) // HELO command must be sent before MAIL FROM
        {
          code503(sock, verbose);
          proceedWithCommand = false;
        }
        if (proceedWithCommand && !reversePath) // MAIL FROM must be sent before RCPT TO
        {
          code503(sock, verbose);
          proceedWithCommand = false;
        }
        if (proceedWithCommand && forwardPaths.empty()) // At least one RCPT TO is required
        {
          code503(sock, verbose, "At least one recipient required");
          proceedWithCommand = false;
        }
        if (proceedWithCommand)
        {
          code354(sock, verbose);
          isInDataMode = true; // Enter DATA mode to receive email body
        }
      }
      else if (strcasecmp(command, "QUIT") == 0)
      {
        code221(sock, verbose);
        isQuit = true;
        break;
      }
      else if (strcasecmp(command, "RSET") == 0)
      {
        if (!isHello) // HELO command must be sent before MAIL FROM
        {
          code503(sock, verbose);
          proceedWithCommand = false;
        }
        if (proceedWithCommand)
        {
          reversePath = NULL;
          forwardPaths.clear();
          code250(sock, verbose);
        }
      }
      else if (strcasecmp(command, "NOOP") == 0)
      {
        if (!isHello) // HELO command must be sent before NOOP
        {
          code503(sock, verbose);
          proceedWithCommand = false;
        }
        if (proceedWithCommand)
        {
          code250(sock, verbose);
        }
      }
      else
      {
        code500(sock, verbose);
      }

      size_t remaining = bufferLength - (crlf + 2 - dynamicBuffer);
      memmove(dynamicBuffer, crlf + 2, remaining);
      bufferLength = remaining;
      dynamicBuffer = (char *)realloc(dynamicBuffer, bufferLength + 1); // Adjust buffer size
      dynamicBuffer[bufferLength] = '\0';
    }

    if (isQuit)
    {
      break;
    }
  }
  // Clean up
  free(dynamicBuffer);

  // Remove socket from the list upon disconnection
  pthread_mutex_lock(&client_sockets_mutex);
  for (int i = 0; i < MAX_CONNECTIONS; i++)
  {
    if (client_sockets[i] == sock)
    {
      client_sockets[i] = 0;
      break;
    }
  }
  pthread_mutex_unlock(&client_sockets_mutex);

  close(sock);
  if (read_size < 0)
  {
    perror("recv failed");
  }

  return 0;
}

void signal_handler(int sig)
{
  if (sig == SIGINT)
  {
    close_all_clients(); // Close all client connections
    close(server_fd);    // Close server socket
    exit(EXIT_SUCCESS);  // Exit program
  }
}

void close_all_clients()
{
  pthread_mutex_lock(&client_sockets_mutex);
  for (int i = 0; i < MAX_CONNECTIONS; i++)
  {
    if (client_sockets[i] != 0)
    {
      write(client_sockets[i], "-ERR Server shutting down\r\n", 27);
      if (verbose)
      {
        fprintf(stderr, "[%d] S: -ERR Server shutting down\n", client_sockets[i]);
        fprintf(stderr, "[%d] Connection closed\n", client_sockets[i]);
      }
      close(client_sockets[i]);
      client_sockets[i] = 0;
    }
  }
  pthread_mutex_unlock(&client_sockets_mutex);
}

// sudo lsof -i -P -n | grep LISTEN