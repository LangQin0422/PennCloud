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

#include "server.hh"
#include "../include/console.hh"
#include "../include/handler.hh"

// create ip:port to ip:port map
std::unordered_map<std::string, std::string> ipPortMap;

int main(int argc, char *argv[])
{
  parseArgs(argc, argv);

  initServer();

  initKVS();

  get("/", handleEntry);

  get("/ping", handlePing);

  get("/kvsTable", handleTable);

  get("/register", handleRegister);

  get("/admin", handleAdmin);

  get("/api/workers", handleAPIWorkers);

  get("/api/kvs", handleAPIKVS);

  post("/api/kvs/kill", handleAPIKillKVS);

  post("/api/kvs/start", handleAPIStartKVS);

  get("/api/kvs/viewRows", handleAPIAllRows);

  checkForInactiveWorkers();

  startServer();

  return 0;
}