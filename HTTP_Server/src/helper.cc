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
#include <netinet/in.h>
#include <sstream>
#include <sys/stat.h>
#include <random>
#include <chrono>
#include <fstream>

#include "../include/server.hh"
#include "../include/requestImp.hh"
#include "../include/responseImp.hh"

std::unordered_map<std::string, std::string> parseQueryParams(const std::string &url)
{
  std::unordered_map<std::string, std::string> queryParams;
  auto qPos = url.find('?');
  if (qPos != std::string::npos)
  {
    std::string queryString = url.substr(qPos + 1);
    std::istringstream queryStream(queryString);
    std::string pair;

    while (std::getline(queryStream, pair, '&'))
    {
      auto eqPos = pair.find('=');
      if (eqPos != std::string::npos)
      {
        std::string key = pair.substr(0, eqPos);
        std::string value = pair.substr(eqPos + 1);
        queryParams[key] = value;
      }
    }
  }
  return queryParams;
}

std::string extractPath(const std::string &fullPath)
{
  auto qPos = fullPath.find('?');
  if (qPos != std::string::npos)
  {
    return fullPath.substr(0, qPos);
  }
  return fullPath;
}

std::vector<std::string> split(const std::string &s, char delimiter)
{
  std::vector<std::string> segments;
  std::string segment;
  std::istringstream segmentStream(s);
  while (std::getline(segmentStream, segment, delimiter))
  {
    segments.push_back(segment);
  }
  return segments;
}

bool matchRoute(const std::string &method, const std::string &requestPath, std::string &matchedPath,
                std::unordered_map<std::string, RouteHandler> &routes,
                std::unordered_map<std::string, std::string> &extractedParams)
{
  auto requestSegments = split(requestPath, '/');

  for (const auto &route : routes)
  {
    auto routeSegments = split(route.first, '/');

    if (routeSegments.size() != requestSegments.size())
    {
      continue; // The paths have different numbers of segments
    }

    bool isMatch = true;
    std::unordered_map<std::string, std::string> currentExtractedParams;

    for (size_t i = 0; i < routeSegments.size(); ++i)
    {
      if (routeSegments[i].front() == ':')
      {
        // Dynamic segment, extract parameter
        std::string paramName = routeSegments[i].substr(1); // Remove the ':' prefix
        currentExtractedParams[paramName] = requestSegments[i];
        if (verbose)
        {
          fprintf(stderr, "Extracted param: %s = %s\n", paramName.c_str(), requestSegments[i].c_str());
        }
      }
      else if (routeSegments[i] != requestSegments[i])
      {
        // Static segment does not match
        isMatch = false;
        break;
      }
    }

    if (isMatch)
    {
      // A matching route is found
      extractedParams = std::move(currentExtractedParams);
      matchedPath = std::move(route.first);
      return true;
    }
  }

  return false;
}

bool fileExists(const std::string &path)
{
  struct stat buffer;
  return (stat(path.c_str(), &buffer) == 0);
}

std::string getMimeType(const std::string &filePath)
{
  std::string extension = filePath.substr(filePath.find_last_of(".") + 1);
  if (extension == "html")
    return "text/html";
  else if (extension == "css")
    return "text/css";
  else if (extension == "png")
    return "image/png";
  else if (extension == "jpg" || extension == "jpeg")
    return "image/jpeg";
  else if (extension == "txt")
    return "text/plain";
  else if (extension == "js")
    return "application/javascript";
  else if (extension == "json")
    return "application/json";
  else if (extension == "pdf")
    return "application/pdf";
  // Default binary file type
  return "application/octet-stream";
}

// trim from start (in place)
inline void ltrim(std::string &s)
{
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch)
                                  { return !std::isspace(ch); }));
}

// trim from end (in place)
inline void rtrim(std::string &s)
{
  s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch)
                       { return !std::isspace(ch); })
              .base(),
          s.end());
}

std::string trim(const std::string &str)
{
  std::string strCopy = str;
  rtrim(strCopy);
  ltrim(strCopy);
  return strCopy;
}

int getContentLength(const std::unordered_map<std::string, std::string> &headers)
{
  auto it = headers.find("Content-Length");
  if (it != headers.end())
  {
    return std::stoi(it->second);
  }
  return 0; // Default to zero if Content-Length is not present
}

std::unordered_map<std::string, std::string> parseHeaders(const std::string &headerStr)
{
  std::unordered_map<std::string, std::string> headers;
  std::istringstream stream(headerStr);
  std::string line;
  std::getline(stream, line);
  while (std::getline(stream, line) && line != "\r")
  {
    auto colonPos = line.find(':');
    if (colonPos != std::string::npos)
    {
      std::string headerName = line.substr(0, colonPos);
      std::string headerValue = trim(line.substr(colonPos + 1));
      // Trim headerValue if necessary
      headers[headerName] = headerValue;
    }
  }
  return headers;
}

std::string generateRandomID(size_t length)
{
  const std::string chars =
      "0123456789"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz";
  std::random_device rd;
  std::mt19937 generator(rd());
  std::uniform_int_distribution<> distribution(0, chars.size() - 1);

  std::string random_string;
  for (size_t i = 0; i < length; ++i)
  {
    random_string += chars[distribution(generator)];
  }

  return random_string;
}

bool isEmptyOrWhitespace(const std::string &str)
{
  return str.empty() || std::all_of(str.begin(), str.end(), isspace);
}

void parseConfig(const std::string &filename, Config &config)
{
  std::ifstream file(filename);
  if (!file.is_open())
  {
    throw std::runtime_error("Could not open file");
  }

  std::string line, sectionName;
  while (std::getline(file, line))
  {
    // Trim whitespace from the beginning and end of the line
    line.erase(0, line.find_first_not_of(" \t"));
    line.erase(line.find_last_not_of(" \t") + 1);

    if (line.empty() || line[0] == ';')
    { // Skip empty lines and comments
      continue;
    }

    if (line.front() == '[' && line.back() == ']')
    { // Section
      sectionName = line.substr(1, line.size() - 2);
    }
    else
    { // Key-value pair
      std::istringstream is_line(line);
      std::string key;
      if (std::getline(is_line, key, '='))
      {
        std::string value;
        std::getline(is_line, value);

        // Remove whitespace around the key and value
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));

        config[sectionName][key] = value;
      }
    }
  }

  file.close();
}