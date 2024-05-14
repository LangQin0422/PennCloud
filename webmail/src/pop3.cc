#include <stdlib.h>
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
#include <set>
#include <iostream>

#include "../include/pop3_msg.hh"
#include "../include/pop3_command.hh"
#include "../include/io_helper.hh"
#include "../include/echoserver.hh"
#include "../include/webmail.hh"

#define DEFAULT_PORT 11000
#define MAX_CONNECTIONS 100
#define BUFFER_SIZE 1024

struct ThreadArgs
{
  int *client_socket;
};

int verbose = 0;                     // Verbose output
int server_fd;                       // Server socket file descriptor
int client_sockets[MAX_CONNECTIONS]; // Active client sockets
pthread_mutex_t client_sockets_mutex = PTHREAD_MUTEX_INITIALIZER;

KVSClient kvsClient;
Config config;

void testSimple()
{
  std::cout << "Testing simple put and get..." << std::endl;
  std::string value;

  kvsClient.Put("row11", "col11", "value1", "LOCK_BYPASS");
  assert(kvsClient.Get("row11", "col11", value, "LOCK_BYPASS"));
  assert(value == "value1");

  kvsClient.Delete("row11", "col11", "LOCK_BYPASS");
  assert(!kvsClient.Get("row11", "col11", value, "LOCK_BYPASS"));

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
  while ((option = getopt(argc, argv, "p:av")) != -1)
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
    default:
      fprintf(stderr, "Usage: %s [-p port] [-a] [-v] <file_path>\n", argv[0]);
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

    // Creating a new thread for each connection
    if (pthread_create(&thread_id, nullptr, handle_client, (void *)args) != 0)
    {
      perror("pthread_create failed");
      close(new_socket);
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
  bool isQuit = false;        // Flag to check if the client has sent a QUIT command

  char buffer[BUFFER_SIZE]; // Temporary buffer to read incoming data
  int read_size;

  // Send the greeting message
  char *greeting = "+OK POP3 ready [penncloud07.com]\r\n";
  write(sock, greeting, strlen(greeting));
  if (verbose)
  {
    fprintf(stderr, "[%d] S: %s", sock, greeting);
  }

  char *user = NULL;
  bool loggedIn = false;
  int fd = -1;
  std::set<unsigned int> deletedMessages;
  std::string mutexId = "-"; // "-" means no mutex has been set. This is the default value int kvsClient functions

  while ((read_size = recv(sock, buffer, BUFFER_SIZE - 1, 0)) > 0)
  {
    buffer[read_size] = '\0';

    // Resize dynamicBuffer to accommodate new data
    dynamicBuffer = (char *)realloc(dynamicBuffer, bufferLength + read_size + 1);
    memcpy(dynamicBuffer + bufferLength, buffer, read_size + 1);
    bufferLength += read_size;

    // Process complete commands from dynamicBuffer
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

      if (command && strcasecmp(command, "USER") == 0)
      {
        if (argument == NULL)
        {
          err_code(sock, verbose, "missing username");
          proceedWithCommand = false;
        }
        if (proceedWithCommand && loggedIn)
        {
          err_code(sock, verbose, "already authenticated");
          proceedWithCommand = false;
        }
        if (proceedWithCommand)
        {
          processUserCommand(sock, strdup(argument), &user, verbose);
        }
      }
      else if (command && strcasecmp(command, "PASS") == 0)
      {
        if (user == NULL)
        {
          err_code(sock, verbose, "No username provided");
          proceedWithCommand = false;
        }
        if (proceedWithCommand && argument == NULL)
        {
          err_code(sock, verbose, "Missing password");
          proceedWithCommand = false;
        }
        if (proceedWithCommand && loggedIn)
        {
          err_code(sock, verbose, "already authenticated");
          proceedWithCommand = false;
        }
        if (proceedWithCommand)
        {
          processPassCommand(sock, strdup(argument), &loggedIn, &user, verbose, mutexId);
        }
      }
      else if (command && strcasecmp(command, "UIDL") == 0)
      {
        if (!loggedIn)
        {
          err_code(sock, verbose, "not authenticated");
          proceedWithCommand = false;
        }
        if (proceedWithCommand)
        {
          processUidlCommand(sock, user, argument, deletedMessages, verbose, mutexId);
        }
      }
      else if (command && strcasecmp(command, "STAT") == 0)
      {
        if (!loggedIn)
        {
          err_code(sock, verbose, "not authenticated");
          proceedWithCommand = false;
        }
        if (proceedWithCommand && argument != NULL)
        {
          err_code(sock, verbose, "STAT command does not take any arguments");
          proceedWithCommand = false;
        }
        if (proceedWithCommand)
        {
          processStatCommand(sock, user, deletedMessages, verbose, mutexId);
        }
      }
      else if (command && strcasecmp(command, "LIST") == 0)
      {
        if (!loggedIn)
        {
          err_code(sock, verbose, "not authenticated");
          proceedWithCommand = false;
        }
        if (proceedWithCommand)
        {
          processListCommand(sock, user, argument, deletedMessages, verbose, mutexId);
        }
      }
      else if (command && strcasecmp(command, "RETR") == 0)
      {
        if (!loggedIn)
        {
          err_code(sock, verbose, "not authenticated");
          proceedWithCommand = false;
        }
        if (proceedWithCommand)
        {
          processRetrCommand(sock, user, argument, deletedMessages, verbose, mutexId);
        }
      }
      else if (command && strcasecmp(command, "DELE") == 0)
      {
        if (!loggedIn)
        {
          err_code(sock, verbose, "not authenticated");
          proceedWithCommand = false;
        }
        if (proceedWithCommand)
        {
          processDeleCommand(sock, user, argument, deletedMessages, verbose, mutexId);
        }
      }
      else if (command && strcasecmp(command, "RSET") == 0)
      {
        if (!loggedIn)
        {
          err_code(sock, verbose, "not authenticated");
          proceedWithCommand = false;
        }
        if (proceedWithCommand && argument != NULL)
        {
          err_code(sock, verbose, "RSET command does not take any arguments");
          proceedWithCommand = false;
        }
        if (proceedWithCommand)
        {
          processRsetCommand(sock, user, deletedMessages, verbose, mutexId);
        }
      }
      else if (command && strcasecmp(command, "NOOP") == 0)
      {
        if (!loggedIn)
        {
          err_code(sock, verbose, "not authenticated");
          proceedWithCommand = false;
        }
        if (proceedWithCommand && argument != NULL)
        {
          err_code(sock, verbose, "NOOP command does not take any arguments");
          proceedWithCommand = false;
        }
        if (proceedWithCommand)
        {
          ok_code(sock, verbose, "");
        }
      }
      else if (command && strcasecmp(command, "QUIT") == 0)
      {
        processQuitCommand(sock, user, deletedMessages, verbose, mutexId);
        isQuit = true;
        break;
      }
      else
      {
        err_code(sock, verbose, "Not supported");
      }

      // Shift any remaining unprocessed data to the beginning of dynamicBuffer
      size_t remaining = bufferLength - (crlf + 2 - dynamicBuffer);
      memmove(dynamicBuffer, crlf + 2, remaining);
      bufferLength = remaining;
      dynamicBuffer = (char *)realloc(dynamicBuffer, bufferLength + 1); // Adjust buffer size
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

/**
 * @brief Handles UNIX signals received by the program. Specifically designed to handle the SIGINT signal,
 *        which is typically sent when the user presses Ctrl+C. Upon receiving SIGINT, this handler
 *        initiates a graceful shutdown of the server by closing all active client connections, closing
 *        the server's main socket, and then exiting the program successfully.
 *
 * @note This function assumes the existence of a `close_all_clients` function, which is responsible for
 *       closing all client connections, a `server_fd` variable that holds the file descriptor for the server's
 *       main listening socket, and that the server is designed to run until interrupted by the user.
 *
 * @param sig The signal code received. This handler specifically checks for SIGINT (interrupt signal).
 *
 * @return void The function does not return a value. Upon receiving SIGINT, it performs the necessary cleanup
 *              and exits the program using `exit(EXIT_SUCCESS)`.
 */
void signal_handler(int sig)
{
  if (sig == SIGINT)
  {
    close_all_clients(); // Close all client connections
    close(server_fd);    // Close server socket
    exit(EXIT_SUCCESS);  // Exit program
  }
}

/**
 * @brief Closes all active client connections gracefully. This function iterates through a global array of client socket descriptors,
 *        sending an error message indicating server shutdown to each connected client before closing the socket. It ensures thread-safe
 *        access to the global client sockets array by acquiring a mutex before performing operations and releasing it afterwards. This
 *        function is typically called during server shutdown procedures to inform all connected clients of the impending shutdown and
 *        to close all open sockets cleanly.
 *
 * @note This function assumes the existence of a global `client_sockets` array, a `client_sockets_mutex` for protecting access to this array,
 *       a predefined `MAX_CONNECTIONS` constant defining the maximum number of simultaneous connections, and a `verbose` flag for logging.
 *
 * @return void The function does not return a value. It iterates through the client sockets, notifies clients of the server shutdown, closes
 *              each socket, and sets the corresponding array entry to 0 to indicate the socket is no longer in use.
 */
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
