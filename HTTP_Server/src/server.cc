#include <iostream>
#include <string>
#include <unordered_map>
#include <sstream>
#include <functional>
#include <signal.h>
#include <thread>
#include <vector>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <algorithm>
#include <pthread.h>
#include <sys/socket.h>
#include <fstream>
#include <sys/stat.h>
#include <cerrno>
#include <sys/socket.h>
#include <netdb.h>

#include "../include/helper.hh"
#include "../include/server.hh"
#include "../include/requestImp.hh"
#include "../include/responseImp.hh"

Config config;
std::string serverName;
std::string workerID;
std::string configPath;

bool verbose = false;
int server_fd;
int port = 8080;
int client_sockets[MAX_CONNECTIONS];
pthread_mutex_t client_sockets_mutex = PTHREAD_MUTEX_INITIALIZER;

std::unordered_map<std::string, WorkerInfo> activeWorkers;
std::mutex workersMutex;

int load;

std::unordered_map<std::string, RouteHandler> getRoutes;
std::unordered_map<std::string, RouteHandler> postRoutes;
std::unordered_map<std::string, RouteHandler> putRoutes;
std::unordered_map<std::string, RouteHandler> delRoutes;

void parseArgs(int argc, char *argv[])
{
  int opt;
  optind = 0;

  while ((opt = getopt(argc, argv, "vn:c:")) != -1)
  {
    switch (opt)
    {
    case 'v':
      verbose = true;
      break;
    case 'n':
      serverName = optarg;
      break;
    case 'c':
      configPath = optarg;
      break;
    default:
      std::cerr << "Usage: " << argv[0] << " [-v] [-n server name]" << std::endl;
      exit(EXIT_FAILURE);
    }
  }

  if (serverName.empty())
  {
    std::cerr << "Error: Server name is mandatory" << std::endl;
    std::cerr << "Usage: " << argv[0] << " [-v] -n <server name>" << std::endl;
    exit(EXIT_FAILURE);
  }

  if (configPath.empty())
  {
    std::cerr << "Error: Config file path is mandatory" << std::endl;
    std::cerr << "Usage: " << argv[0] << " [-v] -n <server name> -c <config file path>" << std::endl;
    exit(EXIT_FAILURE);
  }
  parseConfig(configPath, config);

  if (config.find(serverName) == config.end())
  {
    std::cerr << "Error: Server name not found in config file" << std::endl;
    exit(EXIT_FAILURE);
  }
}

void initServer()
{
  port = std::stoi(config[serverName]["Port"]);

  std::string folderPath = config[serverName]["Folder"];
  std::string fullPath = folderPath + "/_id";

  // Check if folder exists, if not create it
  struct stat info;
  if (stat(folderPath.c_str(), &info) != 0 || !(info.st_mode & S_IFDIR))
  { // Folder does not exist
    // POSIX compliant way, for Windows use _mkdir
    if (mkdir(folderPath.c_str(), 0777) == -1)
    {
      std::cerr << "Error creating directory: " << strerror(errno) << std::endl;
      exit(EXIT_FAILURE);
    }
  }

  std::ifstream idFile(fullPath);

  if (idFile.is_open())
  {
    // Read the existing worker ID from the file
    std::getline(idFile, workerID);
    idFile.close();

    // Check if the ID is empty or consists only of whitespace
    if (isEmptyOrWhitespace(workerID))
    {
      workerID = generateRandomID(32); // Generate a new ID
      std::ofstream outFile(fullPath);
      if (outFile.is_open())
      {
        outFile << workerID;
        outFile.close();
      }
      else
      {
        std::cerr << "Failed to create ID file" << std::endl;
        exit(EXIT_FAILURE);
      }
    }
  }
  else
  {
    workerID = generateRandomID(32);

    // Write the new ID to _id file
    std::ofstream outFile(fullPath);
    if (outFile.is_open())
    {
      outFile << workerID;
      outFile.close();
    }
    else
    {
      std::cerr << "Failed to create ID file" << std::endl;
      exit(EXIT_FAILURE);
    }
  }
}

void startServer()
{

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
  {
    perror("Socket failed");
    exit(EXIT_FAILURE);
  }

  int opt = 1;
  // Enable the reuse of addresses and ports
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0)
  {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }

  signal(SIGINT, signal_handler);
  struct sockaddr_in address;
  int addrlen = sizeof(address);

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

  while (true)
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
    args->file_path = "pass";

    // Creating a new thread for each connection
    if (pthread_create(&thread_id, nullptr, handleRequest, (void *)args) != 0)
    {
      perror("pthread_create failed");
      close(new_socket);
    }
    else
    {
      pthread_detach(thread_id);
    }
  }
}

void *handleRequest(void *args)
{
  load++;
  // Assuming ThreadArgs is already defined and contains the client socket
  ThreadArgs *threadArgs = static_cast<ThreadArgs *>(args);
  int socket = *(threadArgs->client_socket);
  try
  {
    std::vector<char> requestBytes;

    size_t headersEndPos = std::string::npos;
    char buffer[4096];
    bool headersParsed = false;
    std::unordered_map<std::string, std::string> headers;
    int contentLength = 0;
    int totalBytesRead = 0;

    std::string method, path, protocol;
    std::vector<char> body;
    if (verbose)
    {
      fprintf(stderr, "[%d] Reading request\n", socket);
    }
    while (true)
    {
      ssize_t bytesRead = read(socket, buffer, sizeof(buffer));
      if (bytesRead > 0)
      {
        requestBytes.insert(requestBytes.end(), buffer, buffer + bytesRead);
        totalBytesRead += bytesRead;

        if (!headersParsed)
        {
          // Check if we've reached the end of the headers
          auto it = std::search(requestBytes.begin(), requestBytes.end(), CRLFCRLF, CRLFCRLF + 4);
          if (it != requestBytes.end())
          {
            headersEndPos = std::distance(requestBytes.begin(), it) + 4; // +4 for the length of "\r\n\r\n"

            std::string requestStr(requestBytes.begin(), it); // Request line and headers without final CRLFCRLF

            std::istringstream requestStream(requestStr);
            std::string requestLine;
            std::getline(requestStream, requestLine, '\r'); // Extract request line
            std::istringstream requestLineStream(requestLine);
            requestLineStream >> method >> path >> protocol;

            std::string headerStr(requestBytes.begin(), requestBytes.begin() + headersEndPos);
            headers = parseHeaders(headerStr);
            contentLength = getContentLength(headers);
            headersParsed = true;
          }
        }

        if (headersParsed)
        {
          // Check if we've read at least up to the end of the specified content
          if (totalBytesRead >= headersEndPos + contentLength)
          {
            // Extract the body
            auto bodyStart = requestBytes.begin() + headersEndPos;
            // Ensure not to exceed totalBytesRead in case contentLength was incorrectly specified to be larger
            auto bodyEnd = bodyStart + std::min(static_cast<size_t>(contentLength), requestBytes.size() - headersEndPos);
            body = std::vector<char>(bodyStart, bodyEnd);

            // We've read the entire request and can break from the loop
            break;
          }
        }
      }
      else if (bytesRead < 0)
      {
        if (verbose)
        {
          fprintf(stderr, "[%d] Error reading from socket\n", socket);
        }
        break;
      }
    }
    if (verbose)
    {
      fprintf(stderr, "[%d] Request read\n", socket);
    }

    // handle query params
    std::string pathAndQuery = path; // Original path which might contain query params
    path = extractPath(path);        // Clean path without query params
    auto queryParams = parseQueryParams(pathAndQuery);

    sockaddr_in remoteAddr;
    socklen_t addrLen = sizeof(remoteAddr);
    getpeername(socket, (struct sockaddr *)&remoteAddr, &addrLen);

    std::string body_str = std::string(body.begin(), body.end());

    RequestImp req(method, path, protocol, body_str, headers, queryParams, {}, remoteAddr);

    std::unordered_map<std::string, std::string> params;

    if (verbose)
    {
      fprintf(stderr, "[%d] New Request \n", socket);
      req.print();
      fprintf(stderr, "-----------------------------------\n");
    }

    ResponseImp res(socket, method);
    std::string matchedPath;

    if ((method == "GET" || method == "HEAD") && matchRoute(method, path, matchedPath, getRoutes, params))
    {
      req.setParams(params);
      getRoutes[matchedPath](req, res);
    }
    else if (method == "POST" && matchRoute(method, path, matchedPath, postRoutes, params))
    {
      req.setParams(params);
      postRoutes[matchedPath](req, res);
    }
    else if (method == "PUT" && matchRoute(method, path, matchedPath, putRoutes, params))
    {
      req.setParams(params);
      putRoutes[matchedPath](req, res);
    }
    else if (method == "DELETE" && matchRoute(method, path, matchedPath, delRoutes, params))
    {
      req.setParams(params);
      delRoutes[matchedPath](req, res);
    }
    else if (method == "GET")
    {

      std::string filePath = "./public" + path;
      if (verbose)
      {
        fprintf(stderr, "[%d] S: GET %s\n", socket, filePath.c_str());
        fprintf(stderr, "file path is %d\n", fileExists(filePath));
      }
      if (fileExists(filePath))
      {
        if (verbose)
        {
          fprintf(stderr, "[%d] S: 200 OK on get local file\n", socket);
        }
        std::string mimeType = getMimeType(filePath);
        std::ifstream file(filePath, std::ios::binary);
        std::vector<char> content((std::istreambuf_iterator<char>(file)), {});

        res.status(200, "OK");
        res.body(std::string(content.begin(), content.end()));
        res.type(mimeType);
        res.flush();
        if (verbose)
        {
          fprintf(stderr, "[%d] %s\n", socket, res.formatHTML().c_str());
          fprintf(stderr, "-----------------------------------\n");
        }
      }
      else
      {
        std::ifstream file("./public/redirected.html");
        fprintf(stderr, "file path is %d\n", fileExists("./public/redirected.html"));
        if (file.is_open())
        {
          // Read the file content into a string stream
          std::ostringstream buffer;
          buffer << file.rdbuf();
          std::string fileContent = buffer.str();
          fprintf(stderr, "file content is %s\n", fileContent.c_str());
          // Set the response body to the content of the file
          res.body(fileContent);

          // Set the response headers
          res.type("text/html");
          res.status(200, "OK");
          res.flush();
        }
        else
        {
          // If the file cannot be opened, set an error response
          res.status(404, "Not Found");
          res.body("404 Not Found: File not found");
          res.flush();
        }
      }
    }
    else
    {
      // not implement
      res.status(404, "Not Found");
      res.body("Method not implemented or the path is not found");
      res.type("text/plain");
      res.flush();
    }
  }
  catch (const std::exception &e)
  {
    std::cerr << "Exception in handleRequest: " << e.what() << std::endl;
  }
  close(socket);
}

void get(const std::string &path, RouteHandler handler)
{
  getRoutes[path] = handler;
}

void post(const std::string &path, RouteHandler handler)
{
  postRoutes[path] = handler;
}

void put(const std::string &path, RouteHandler handler)
{
  putRoutes[path] = handler;
}

void del(const std::string &path, RouteHandler handler)
{
  delRoutes[path] = handler;
}

void head(const std::string &path, RouteHandler handler)
{
  getRoutes[path] = handler;
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
      // zxiao: more to be done, some of the clients are still waiting for response,
      // it should write a response to the client before closing the connection in html
      write(client_sockets[i], "HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\n\r\n", 56);

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

void sendHTTPRequest(const std::string &ipAddress, int port, const std::string &path)
{
  struct sockaddr_in serv_addr;

  // Create a socket
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
  {
    std::cerr << "ERROR opening socket" << std::endl;
    return;
  }

  // Fill in the structure "serv_addr" with the address of the server
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(ipAddress.c_str()); // Convert IP address to network byte order
  serv_addr.sin_port = htons(port);                         // Convert port number to network byte order

  // Connect to the server
  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
  {
    std::cerr << "ERROR connecting" << std::endl;
    close(sockfd);
    return;
  }

  // Send an HTTP request to the server
  std::string request = "GET " + path + " HTTP/1.1\r\n";
  request += "Host: " + ipAddress + "\r\n";
  request += "Connection: close\r\n\r\n";

  if (write(sockfd, request.c_str(), request.length()) < 0)
  {
    std::cerr << "ERROR writing to socket" << std::endl;
    close(sockfd);
    return;
  }

  // Close the socket
  close(sockfd);
}

void pingMaster(const std::string &localWorkerID, const std::string &localPort)
{
  while (true)
  {
    try
    {
      std::string path = "/ping?id=" + localWorkerID + "&port=" + localPort + "&load=" + std::to_string(load);

      sendHTTPRequest(config["frontend.coordinator"]["IP"], std::stoi(config["frontend.coordinator"]["Port"]), path);

      std::this_thread::sleep_for(std::chrono::seconds(std::stoi(config[serverName]["PingTime"])));
    }
    catch (const std::exception &e)
    {
      std::cerr << "Exception in ping thread: " << e.what() << std::endl;
      break;
    }
  }
}

void startPingThread()
{
  std::string localWorkerID = workerID;
  std::string localPort = std::to_string(port);

  std::thread([localWorkerID, localPort]()
              { pingMaster(localWorkerID, localPort); })
      .detach();
  // start a thread that set load be zero every 30 seconds
  std::thread([]()
              {
                while (true)
                {
                  std::this_thread::sleep_for(std::chrono::seconds(std::stoi(config[serverName]["LoadCleanupTime"])));
                  // make load decay by half
                  load = load / 2;
                } })
      .detach();
}

void removeInactiveWorkers()
{
  int timeoutSec;

  if (config[serverName].find("WorkerTimeout") != config[serverName].end())
  {
    timeoutSec = std::stoi(config[serverName]["WorkerTimeout"]);
  }
  else
  {
    timeoutSec = 30;
  }
  const std::chrono::seconds timeout(timeoutSec);
  while (true)
  {
    auto now = std::chrono::steady_clock::now();
    {
      std::lock_guard<std::mutex> lock(workersMutex);
      for (auto it = activeWorkers.begin(); it != activeWorkers.end();)
      {
        if (now - it->second.lastPingTime > timeout)
        {
          if (verbose)
            std::cerr << "Worker at " << it->first << " is inactive and being marked in activative\n";
          it->second.alive = false;
          ++it;
        }
        else
        {
          ++it;
        }
      }
    }

    std::this_thread::sleep_for(std::chrono::seconds(5));
  }
}

void checkForInactiveWorkers()
{
  std::thread([]()
              { removeInactiveWorkers(); })
      .detach();
}
