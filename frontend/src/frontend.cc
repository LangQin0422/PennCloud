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

#include "../include/frontend.hh"

#include "server.hh"
#include "responseImp.hh"
#include "loginService.hh"
#include "webmailService.hh"
#include "webstorageService.hh"
#include <cassert>

KVSClient kvsClient;

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
  parseArgs(argc, argv);

  initServer();

  initKVS();

  get("/", handleIndexPage);

  post("/login", handleLogin);

  post("/signup", handleSignUp);

  del("/logout", handleLogout);

  get("/isLoggedIn", handleIsLoggedIn);

  put("/changePassword", handleChangePassword);

  get("/emails", handleGetEmails);

  del("/emails/:id", handleDeleteEmail);

  post("/emails", handleSendEmail);

  get("/files", handleGetFiles);

  get("/files/:name", handleGetFile);

  post("/files", handleUploadFile);

  del("/files/:name", handleDeleteFile);

  put("/files/:name", handleMoveFile);

  put("/files/:name/:newName", handleRenameFile);

  get("/folders", handleGetFolders);

  post("/folders", handleCreateFolder);

  del("/folders/:name", handleDeleteFolder);

  put("/folders/:name", handleMoveFolder);

  put("/folders/:name/:newName", handleRenameFolder);

  startPingThread();

  startServer();

  std::thread(removeExpiredSessions).detach();

  return 0;
}
