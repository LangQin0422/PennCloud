#include <iostream>
#include <string>
#include <unordered_map>
#include <functional>
#include <thread>
#include <vector>
#include <netinet/in.h>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <cstring>

#include "../include/server.hh"

void handleIndexPage(const Request &request, Response &response)
{
  // Open the file
  response.body("Hello World!");

  // Set the response headers
  response.type("text/html");
  response.status(200, "OK");

  // Flush the response
  response.flush();
}

int main(int argc, char *argv[])
{
  parseArgs(argc, argv);

  initServer();

  get("/", handleIndexPage);

  startPingThread();

  startServer();

  return 0;
}