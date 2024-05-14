#pragma once

#ifndef SERVER_HH
#define SERVER_HH

#include <unordered_map>
#include <functional>
#include <mutex>

#include "request.hh"
#include "response.hh"

// #include "KVSCTRLClient.hpp"
// #include "KVSClient.hpp"

#define MAX_CONNECTIONS 100
#define CRLF "\r\n"
#define CRLFCRLF "\r\n\r\n"

struct ThreadArgs
{
  int *client_socket;
  std::string file_path;
};

struct WorkerInfo
{
  std::string ip;
  int port;
  int load; // Number of requests handled in the last interval
  std::chrono::steady_clock::time_point lastPingTime;
  bool alive = true;
};

using RouteHandler = std::function<void(const Request &, Response &)>;
using Section = std::unordered_map<std::string, std::string>;
using Config = std::unordered_map<std::string, Section>;

extern std::unordered_map<std::string, RouteHandler> getRoutes;
extern std::unordered_map<std::string, RouteHandler> postRoutes;
extern std::unordered_map<std::string, RouteHandler> putRoutes;
extern std::unordered_map<std::string, RouteHandler> delRoutes;

extern int server_fd;
extern int port;
extern bool verbose;

extern int load;

extern Config config;
extern std::string configPath;
extern std::string serverName;
extern std::string workerID;

extern std::unordered_map<std::string, WorkerInfo> activeWorkers;
extern std::mutex workersMutex;

// extern KVSCTRLClient kvsCtrlClient;
// extern KVSClient kvsClient;
/**
 * Parses command line arguments.
 *
 * This function parses the command line arguments provided to the program. It supports two options:
 * '-v' for enabling verbose mode and '-p port' for specifying the port number.
 * If the verbose option is provided, the global variable 'verbose' is set to true.
 * If the port option is provided, the global variable 'port' is set to the specified port number.
 * If an unknown option is provided or if the option requires an argument that is not provided,
 * it prints the correct usage of the program and exits with a failure status.
 *
 * @param argc The number of command line arguments.
 * @param argv An array of command line argument strings.
 */
void parseArgs(int argc, char *argv[]);

/**
 * Starts the HTTP server.
 *
 * This function creates a socket, binds it to the specified port, and listens for incoming connections.
 * Once a connection is accepted, it spawns a new thread to handle the request.
 * It continues listening for connections until interrupted by a signal.
 */
void startServer();

/**
 * Handle an incoming HTTP request.
 *
 * This function is executed in a separate thread for each incoming connection.
 * It reads the request from the client socket, parses it, and dispatches it to the appropriate route handler.
 * If no matching route is found, it sends a 404 Not Found response back to the client.
 *
 * @param args A pointer to a ThreadArgs structure containing the client socket.
 * @return nullptr
 */
void *handleRequest(void *args);

/**
 * Register a GET route with the specified path and handler function.
 *
 * @param path The URL path for the GET route.
 * @param handler The handler function to be executed when the specified path is requested via GET method.
 */
void get(const std::string &path, RouteHandler handler);

/**
 * Register a POST route with the specified path and handler function.
 *
 * @param path The URL path for the POST route.
 * @param handler The handler function to be executed when the specified path is requested via POST method.
 */
void post(const std::string &path, RouteHandler handler);

/**
 * Register a PUT route with the specified path and handler function.
 *
 * @param path The URL path for the PUT route.
 * @param handler The handler function to be executed when the specified path is requested via PUT method.
 */
void put(const std::string &path, RouteHandler handler);

/**
 * Register a DELETE route with the specified path and handler function.
 *
 * @param path The URL path for the DELETE route.
 * @param handler The handler function to be executed when the specified path is requested via DELETE method.
 */
void del(const std::string &path, RouteHandler handler);

/**
 * Register a HEAD route with the specified path and handler function.
 *
 * @param path The URL path for the HEAD route.
 * @param handler The handler function to be executed when the specified path is requested via HEAD method.
 */
void head(const std::string &path, RouteHandler handler);

/**
 * Signal handler function to handle SIGINT (Ctrl+C) signal.
 * Closes all client connections and the server socket, then exits the program.
 *
 * @param sig The signal number.
 */
void signal_handler(int sig);

/**
 * Closes all client connections gracefully by sending a 503 Service Unavailable response
 * to each client before closing the connection.
 */
void close_all_clients();

/**
 * Initializes the server based on configuration settings.
 * Reads or generates a worker ID and creates necessary folders.
 */
void initServer();

/**
 * Starts a thread to ping the master with worker ID and port.
 * Starts another thread to set load to zero periodically.
 */
void startPingThread();

/**
 * Starts a thread to periodically check for and remove inactive workers.
 */
void checkForInactiveWorkers();

#endif // SERVER_HH