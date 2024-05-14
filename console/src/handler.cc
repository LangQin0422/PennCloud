#include <sstream>
#include <fstream>

#include "../include/console.hh"
#include "../include/handler.hh"

KVSClient kvsClient;
KVSCTRLClient kvsCtrlClient;

const bool localKVS = false;

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
  std::string load_str = request.queryParams("load");

  if (load_str.empty())
  {
    response.status(400, "Bad Request");
    response.body("Missing load parameter");
    response.flush();
    return;
  }

  int load = std::stoi(load_str);

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
  std::string client = request.ip() + ":" + std::to_string(request.port());

  // check if client in ipPortMap or not
  if (ipPortMap.find(client) != ipPortMap.end())
  {
    if (verbose)
    {
      fprintf(stderr, "Client %s already registered\n", client.c_str());
    }
    response.body(ipPortMap[client]);
  }
  else
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

    if (verbose)
    {
      fprintf(stderr, "Redirecting to %s\n", redirectUrl.c_str());
    }

    response.body(redirectUrl);
    ipPortMap[client] = redirectUrl;
  }
  response.type("text/html");
  response.header("Access-Control-Allow-Origin", "*");                   // Allow any domain
  response.header("Access-Control-Allow-Methods", "GET, POST, OPTIONS"); // Allowed methods
  response.header("Access-Control-Allow-Headers", "Content-Type, Authorization");
  response.status(200, "OK");
  response.flush();
}

void handleAdmin(const Request &request, Response &response)
{
  std::ifstream file("./public/admin.html");
  if (file.is_open())
  {
    // Read the file content into a string stream
    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string fileContent = buffer.str();

    // Set the response body to the content of the file
    response.body(fileContent);

    // Set the response headers
    response.type("text/html");
    response.status(200, "OK");
  }
  else
  {
    // If the file cannot be opened, set an error response
    response.status(404, "Not Found");
    response.body("404 Not Found: File not found");
  }

  // Flush the response
  response.flush();
}

void handleTable(const Request &request, Response &response)
{
  std::string kvsIP = request.queryParams("kvsIP");
  std::ifstream file("./public/viewTable.html");
  if (file.is_open())
  {
    std::ostringstream buffer;
    buffer << file.rdbuf(); // Read the file content into a string stream
    std::string fileContent = buffer.str();

    // Replace the placeholder with the actual kvsIP value
    size_t pos;
    // change to while loop
    while ((pos = fileContent.find("{{kvsIP}}")) != std::string::npos)
    {
      fileContent.replace(pos, 9, kvsIP);
    }

    response.body(fileContent);
    response.type("text/html");
    response.status(200, "OK");
  }
  else
  {
    response.status(404, "Not Found");
    response.body("404 Not Found: File not found");
  }
  response.flush();
}

void handleEntry(const Request &request, Response &response)
{
  std::ifstream file("./public/index.html");
  if (file.is_open())
  {
    // Read the file content into a string stream
    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string fileContent = buffer.str();

    // Set the response body to the content of the file
    response.body(fileContent);

    // Set the response headers
    response.type("text/html");
    response.status(200, "OK");
  }
  else
  {
    // If the file cannot be opened, set an error response
    response.status(404, "Not Found");
    response.body("404 Not Found: File not found");
  }

  // Flush the response
  response.flush();
}

void handleAPIWorkers(const Request &request, Response &response)
{
  std::stringstream jsonStream;
  jsonStream << "[";

  std::lock_guard<std::mutex> lock(workersMutex);
  for (auto it = activeWorkers.begin(); it != activeWorkers.end(); ++it)
  {
    auto lastPingTime = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::steady_clock::now() - it->second.lastPingTime)
                            .count();
    std::string status = it->second.alive ? "Alive" : "Inactive";
    int loadPercent = it->second.load; // Ensure the percent does not exceed 100

    jsonStream << (it != activeWorkers.begin() ? ", " : "") // Add comma between items, except before the first
               << "{"
               << "\"id\": \"" << it->first << "\","
               << "\"ip\": \"" << it->second.ip << "\","
               << "\"port\": " << it->second.port << ","
               << "\"lastPing\": " << lastPingTime << ","
               << "\"status\": \"" << status << "\","
               << "\"loadPercent\": " << loadPercent
               << "}";
  }

  jsonStream << "]";
  response.body(jsonStream.str());
  response.type("application/json");
  response.status(200, "OK");
  response.flush();
}

void handleAPIKVS(const Request &request, Response &response)
{
  try
  {
    std::vector<std::string> kvsWorkers = {
        "34.171.122.180:50051", "34.171.122.180:50052", "34.171.122.180:50053", "34.70.254.14:50051", "34.70.254.14:50052", "34.70.254.14:50053"};
    if (localKVS) {
      kvsWorkers = splitString(config["kvs.controller"]["KVSWorkerIPs"], ' ');
    }
    // Retrieve servers from KVSCTRLClient
    std::vector<std::string> activeServers = kvsCtrlClient.GetAll();

    // if activeServers is empty, create an empty vector
    if (activeServers.empty())
    {
      activeServers = std::vector<std::string>(kvsWorkers.size(), "");
    }

    // Construct JSON structure
    std::stringstream jsonStream;
    jsonStream << "[";

    for (size_t i = 0; i < kvsWorkers.size(); ++i)
    {
      std::string serverStatus = "inactive"; // Default status

      // Check if the server is in the activeServers list
      auto it = std::find(activeServers.begin(), activeServers.end(), kvsWorkers[i]);
      if (it != activeServers.end())
      {
        serverStatus = "alive"; // Set status to alive if server is active
      }
      else
      {
        // Check if the server is in the kvsWorkers list but not in the activeServers list
        it = std::find(kvsWorkers.begin(), kvsWorkers.end(), activeServers[i]);
        if (it != kvsWorkers.end())
        {
          serverStatus = "inactive"; // Set status to inactive if server is in kvsWorkers but not active
        }
      }

      jsonStream << (i != 0 ? ", " : "")
                 << "{"
                 << "\"id\": \"" << i << "\","
                 << "\"server\": \"" << kvsWorkers[i] << "\","
                 << "\"status\": \"" << serverStatus << "\"" // Add status attribute
                 << "}";
    }

    jsonStream << "]";

    // Set response body with JSON structure
    response.body(jsonStream.str());
    response.type("application/json");
    response.status(200, "OK");
    response.flush();
  }
  catch (const std::exception &e)
  {
    // Handle exceptions
    response.body("Error: " + std::string(e.what()));
    response.type("text/html");
    response.status(500, "Internal Server Error");
    response.flush();
  }
}

void handleAPIKillKVS(const Request &request, Response &response)
{
  try
  {
    // Retrieve server ID from query parameters
    std::string id = request.queryParams("workerID");

    // Kill server with the specified ID
    if (kvsCtrlClient.StopServer(id) == grpc::StatusCode::OK)
    {
      response.body("Server with ID " + id + " killed successfully");
      response.type("text/html");
      response.status(200, "OK");
      response.flush();
    }
    else
    {
      response.body("Error: Server with ID " + id + " could ");
      response.type("text/html");
      response.status(500, "Internal Server Error");
      response.flush();
    }
  }
  catch (const std::exception &e)
  {
    // Handle exceptions
    response.body("Error: " + std::string(e.what()));
    response.type("text/html");
    response.status(500, "Internal Server Error");
    response.flush();
  }
}

void handleAPIStartKVS(const Request &request, Response &response)
{
  try
  {
    // Retrieve server ID from query parameters
    std::string ip = request.queryParams("workerID");

    std::vector<std::string> kvsWorkers = {
        "34.171.122.180:50051", "34.171.122.180:50052", "34.171.122.180:50053", "34.70.254.14:50051", "34.70.254.14:50052", "34.70.254.14:50053"};

    if (localKVS) {
      kvsWorkers = splitString(config["kvs.controller"]["KVSWorkerIPs"], ' ');
    }
    
    // find the index of ip in kvsWorkers
    int id = -1;
    for (int i = 0; i < kvsWorkers.size(); i++)
    {
      if (kvsWorkers[i] == ip)
      {
        id = i;
        break;
      }
    }

    // Start server with the specified ID
    if (id != -1 && kvsCtrlClient.StartServer(id, kvsWorkers) == grpc::StatusCode::OK)
    {
      response.body("Server with ID " + std::to_string(id) + " started successfully");
      response.type("text/html");
      response.status(200, "OK");
      response.flush();
    }
    else
    {
      response.body("Error: Server with ID " + std::to_string(id) + " could not be started");
      response.type("text/html");
      response.status(500, "Internal Server Error");
      response.flush();
    }
  }
  catch (const std::exception &e)
  {
    // Handle exceptions
    response.body("Error: " + std::string(e.what()));
    response.type("text/html");
    response.status(500, "Internal Server Error");
    response.flush();
  }
}

std::string escapeJSON(const std::string &s)
{
  std::string escaped;
  escaped.reserve(s.length()); // Reserve initial length of the input to optimize performance

  for (char c : s)
  {
    switch (c)
    {
    case '\"':
      escaped += "\\\"";
      break;
    case '\\':
      escaped += "\\\\";
      break;
    case '\b':
      escaped += "\\b";
      break;
    case '\f':
      escaped += "\\f";
      break;
    case '\n':
      escaped += "<LF>";
      break;
    case '\r':
      escaped += "<CR>";
      break;
    case '\t':
      escaped += "\\t";
      break;
    default:
      if (c < '\x20' || c > '\x7e')
      {
        // Produce a unicode escape for control characters
        char buffer[128];
        sprintf(buffer, "\\u%04x", c);
        escaped += buffer;
      }
      else
      {
        escaped.push_back(c);
      }
      break;
    }
  }
  return escaped;
}

void handleAPIAllRows(const Request &request, Response &response)
{
  // Default values
  int page = 0;
  int offset = 10;
  std::string kvsIP = "";
  // Try to read 'page' and 'offset' from the request parameters
  if (!request.queryParams("page").empty())
  {
    page = std::stoi(request.queryParams("page"));
  }

  if (!request.queryParams("offset").empty())
  {
    offset = std::stoi(request.queryParams("offset"));
  }

  if (!request.queryParams("kvsIP").empty())
  {
    kvsIP = request.queryParams("kvsIP");
  }

  try
  {
    std::vector<std::string> rows;
    if (kvsIP.empty())
    {
      kvsClient.GetAllRows(rows);
    }
    else
    {
      kvsClient.GetAllRows(rows, kvsIP);
    }

    int start_index = page * offset;
    int end_index = std::min((page + 1) * offset, static_cast<int>(rows.size()));

    std::stringstream jsonStream;
    jsonStream << "[";

    for (int i = start_index; i < end_index; ++i)
    {
      if (i > start_index)
      {
        jsonStream << ", ";
      }
      jsonStream << "{";
      jsonStream << "\"row\": \"" << escapeJSON(rows[i]) << "\", ";
      jsonStream << "\"columns\": {";

      std::vector<std::string> cols;
      if (kvsIP.empty())
      {
        kvsClient.GetColsInRow(rows[i], cols);
      }
      else
      {
        kvsClient.GetColsInRow(rows[i], cols, "-", kvsIP);
      }
      for (size_t j = 0; j < cols.size(); ++j)
      {
        if (j > 0)
        {
          jsonStream << ", ";
        }
        std::string col_value;
        bool success = kvsClient.Get(rows[i], cols[j], col_value);
        // assert(success);
        if (col_value.length() > 100)
        {
          col_value = col_value.substr(0, 100);
        }
        // check row[i] ends with .mbox
        if (rows[i].find(".mbox") != std::string::npos)
        {
          jsonStream << "\"" << escapeJSON(cols[j]) << "\": \"" << escapeJSON(col_value) << "\"";
        }
        else
        {
          jsonStream << "\"" << escapeJSON(cols[j]) << "\": \"" << col_value << "\"";
        }
      }
      jsonStream << "}";
      jsonStream << "}";
    }

    jsonStream << "]";

    // Set the response body, content type, status, and send
    response.body(jsonStream.str());
    response.type("application/json");
    response.status(200, "OK");
    response.flush();
  }
  catch (const std::exception &e)
  {
    response.body("Error: " + std::string(e.what()));
    response.type("application/json");
    response.status(500, "Internal Server Error");
    response.flush();
  }
}

std::vector<std::string> splitString(const std::string &input, char delimiter)
{
  std::vector<std::string> tokens;
  std::istringstream iss(input);
  std::string token;

  while (std::getline(iss, token, delimiter))
  {
    tokens.push_back(token);
  }

  return tokens;
}

void initKVS()
{
  std::vector<std::string> controllers;
  std::vector<std::vector<std::string>> clusters;

  if (localKVS) {
    controllers = {"127.0.0.1:40050"};
    clusters = {{"127.0.0.1:50051", "127.0.0.1:50052", "127.0.0.1:50053"}};
  } else {
    controllers = {"34.171.122.180:40050", "34.70.254.14:40050"};
    clusters = {
        {"34.171.122.180:50051", "34.171.122.180:50052", "34.171.122.180:50053"},
        {"34.70.254.14:50051", "34.70.254.14:50052", "34.70.254.14:50053"}};
  }

  kvsCtrlClient = KVSCTRLClient(controllers);
  kvsClient = KVSClient(clusters);

  std::cout << "Controller starting servers..." << std::endl;

  for (std::vector<std::string> cluster : clusters)
  {
    assert(kvsCtrlClient.StartServer(0, cluster) == grpc::StatusCode::OK || kvsCtrlClient.StartServer(0, cluster) == grpc::StatusCode::ALREADY_EXISTS);
    assert(kvsCtrlClient.StartServer(1, cluster) == grpc::StatusCode::OK || kvsCtrlClient.StartServer(1, cluster) == grpc::StatusCode::ALREADY_EXISTS);
    assert(kvsCtrlClient.StartServer(2, cluster) == grpc::StatusCode::OK || kvsCtrlClient.StartServer(2, cluster) == grpc::StatusCode::ALREADY_EXISTS);
  }

  // fprintf(stderr, "Initializing KVS\n");
  // std::string kvsctr1 = config["kvs.controller"]["IP"] + ":" + config["kvs.controller"]["Port"];
  // std::vector<std::string> kvsCtrList = {kvsctr1};

  // if (verbose)
  //   fprintf(stderr, "KVS Controllers: %s %s\n", kvsctr1.c_str());

  // try
  // {
  //   kvsCtrlClient = KVSCTRLClient(kvsCtrList);
  // }
  // catch (const std::exception &e)
  // {
  //   fprintf(stderr, "Error in starting KVS Controller\n");
  //   return;
  // }

  // std::vector<std::string> kvsWorkers1 = splitString(config["kvs.controller"]["KVSWorkerIPs"], ' ');

  // if (verbose)
  //   fprintf(stderr, "KVS Workers: ");

  // for (int i = 0; i < kvsWorkers1.size(); i++)
  // {
  //   if (verbose)
  //     fprintf(stderr, "%s\n", kvsWorkers1[i].c_str());

  //   int status = kvsCtrlClient.StartServer(i, kvsWorkers1);
  //   if (verbose)
  //     fprintf(stderr, "Status: %d %d\n", status, grpc::StatusCode::OK);
  //   try
  //   {
  //     assert(status == grpc::StatusCode::OK || status == grpc::StatusCode::ALREADY_EXISTS);
  //   }
  //   catch (const std::exception &e)
  //   {
  //     fprintf(stderr, "Error in starting server %s\n", kvsWorkers1[i].c_str());
  //   }
  // }

  // std::vector<std::vector<std::string>> clusters = {kvsWorkers1};

  // kvsClient = KVSClient(clusters);
}
