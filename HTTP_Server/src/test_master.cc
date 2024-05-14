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

void handlePing(const Request &request, Response &response)
{
  std::string id = request.queryParams("id");
  std::string port = request.queryParams("port");
  std::string ip = request.ip();

  int load = std::stoi(request.queryParams("load"));

  std::string uniqueKey = ip + ":" + port;

  {
    std::lock_guard<std::mutex> lock(workersMutex);
    // Check if worker exists or create a new one if it does not
    auto &worker = activeWorkers[id];
    worker.lastPingTime = std::chrono::steady_clock::now();
    worker.load = load;
    worker.port = std::stoi(port);
    worker.ip = ip;
    worker.alive = true;
  }
  if (verbose)
    fprintf(stderr, "Received ping from worker %s on %s\n", id.c_str(), uniqueKey.c_str());
  response.body("");
  response.type("text/html");
  response.status(200, "OK");
  response.flush();
}

void handleRegister(const Request &request, Response &response)
{
  std::lock_guard<std::mutex> lock(workersMutex);

  const WorkerInfo *leastBusyWorker = nullptr;
  for (const auto &worker : activeWorkers)
  {
    if (worker.second.alive == false)
    {
      continue;
    }
    if (leastBusyWorker == nullptr || worker.second.load < leastBusyWorker->load)
    {
      leastBusyWorker = &worker.second;
    }
  }

  std::string redirectUrl = "http://" + leastBusyWorker->ip + ":" + std::to_string(leastBusyWorker->port);

  response.body(redirectUrl);
  response.type("text/html");
  response.status(200, "OK");
  response.flush();
}

void handleAdmin(const Request &request, Response &response)
{
  std::stringstream html;
  html << "<!DOCTYPE html><html><head><title>Active Workers</title>"
       << "<style>"
       << "table {width: 100%; border-collapse: collapse;}"
       << "th, td {border: 1px solid #ddd; padding: 8px; text-align: left;}"
       << "th {background-color: #f2f2f2;}"
       << "</style></head><body>"
       << "<h2>Frontend Servers</h2>"
       << "<table>"
       << "<tr><th>Worker ID</th><th>Last Ping</th><th>Status</th><th>Load</th></tr>";

  html << "<style>"
       << "table {width: 100%; border-collapse: collapse;}"
       << "th, td {border: 1px solid #ddd; padding: 8px; text-align: left;}"
       << "th {background-color: #f2f2f2;}"
       << ".worker-id {max-width: 250px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap;}"
       << ".load-bar {width: 100px; background-color: #ddd; position: relative;}"
       << ".load-fill {height: 20px; background-color: #337ab7;}"
       << ".load-text {position: absolute; left: 50%; transform: translateX(-50%); color: black;}"
       << "</style>";

  // std::lock_guard<std::mutex> lock(workersMutex);
  for (const auto &worker : activeWorkers)
  {
    auto lastPingTime = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::steady_clock::now() - worker.second.lastPingTime)
                            .count();
    std::string status = worker.second.alive ? "Alive" : "Inactive";
    std::string workerUrl = "http://" + worker.second.ip + ":" + std::to_string(worker.second.port);
    int loadPercent = std::min(worker.second.load, 100); // Ensure the percent does not exceed 100

    html << "<tr>"
         << "<td class='worker-id'><a href=\"" << workerUrl << "\">" << worker.first << "</a></td>"
         << "<td>" << lastPingTime << " seconds ago</td>"
         << "<td>" << status << "</td>"
         << "<td>"
         << "<div class='load-bar'>"
         << "<span class='load-text'>" << loadPercent << "%</span>"
         << "<div class='load-fill' style='width: " << loadPercent << "%;'></div>"
         << "</div>"
         << "</td>"
         << "</tr>";
  }

  html << "</table>";

  html << "<h2> Backend Servers </h2>"
       << "<table>"
       << "<tr><th>Worker ID</th><th>Last Ping</th><th>Status</th><th>Load</th></tr>";

  html << "</body></html>";

  response.body(html.str());
  response.type("text/html");
  response.status(200, "OK");
  response.flush();
}

int main(int argc, char *argv[])
{
  parseArgs(argc, argv);

  initServer();

  get("/", handleIndexPage);

  get("/ping", handlePing);

  get("/register", handleRegister);

  get("/admin", handleAdmin);

  checkForInactiveWorkers();

  startServer();

  return 0;
}