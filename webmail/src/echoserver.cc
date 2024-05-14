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
#include "../include/echoserver.hh"

#define DEFAULT_PORT 10000
#define MAX_CONNECTIONS 100
#define BUFFER_SIZE 1024

int verbose = 0;                     // Verbose output
int server_fd;                       // Server socket file descriptor
int client_sockets[MAX_CONNECTIONS]; // Active client sockets
pthread_mutex_t client_sockets_mutex = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char *argv[])
{
  struct sockaddr_in address;
  int addrlen = sizeof(address);
  int port = DEFAULT_PORT;
  int option;

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
      fprintf(stderr, "Usage: %s [-p port] [-a] [-v]\n", argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  // Register signal handler for SIGINT
  signal(SIGINT, signal_handler);

  // Creating socket file descriptor
  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
  {
    perror("Socket failed");
    exit(EXIT_FAILURE);
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

    // Creating a new thread for each connection
    if (pthread_create(&thread_id, NULL, handle_client, (void *)new_sock) != 0)
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

void *handle_client(void *client_socket)
{
  int sock = *(int *)client_socket;
  free(client_socket);

  char *dynamicBuffer = NULL; // Buffer to accumulate data
  size_t bufferLength = 0;    // Current length of the buffer
  bool isQuit = false;        // Flag to check if the client has sent a QUIT command

  char buffer[BUFFER_SIZE]; // Temporary buffer to read incoming data
  int read_size;

  // Send the greeting message
  char *greeting = "+OK Server ready (Author: Zhengyi Xiao / zxiao98)\r\n";
  write(sock, greeting, strlen(greeting));
  if (verbose)
  {
    fprintf(stderr, "[%d] S: %s", sock, greeting);
  }

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
      *crlf = '\0';

      char *command = strtok(dynamicBuffer, " ");
      char *text = command ? strtok(NULL, "") : NULL;

      if (command && verbose)
      {
        fprintf(stderr, "[%d] C: %s %s\n", sock, command, text ? text : "");
      }

      if (command && strcasecmp(command, "ECHO") == 0 && text)
      {
        char response[BUFFER_SIZE];
        snprintf(response, sizeof(response), "+OK %s\r\n", text);
        write(sock, response, strlen(response));
        if (verbose)
        {
          fprintf(stderr, "[%d] S: %s", sock, response);
        }
      }
      else if (command && strcasecmp(command, "QUIT") == 0)
      {
        write(sock, "+OK Goodbye!\r\n", 14);
        if (verbose)
        {
          fprintf(stderr, "[%d] S: +OK Goodbye!\n", sock);
          fprintf(stderr, "[%d] Connection closed\n", sock);
        }
        isQuit = true;
        break;
      }
      else if (command)
      {
        write(sock, "-ERR Unknown command\r\n", 22);
        if (verbose)
        {
          fprintf(stderr, "[%d] S: -ERR Unknown command\n", sock);
        }
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