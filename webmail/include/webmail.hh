#pragma once

#include "KVSClient.hpp"
#include <fstream>
#include <sstream>

extern KVSClient kvsClient;
using Section = std::unordered_map<std::string, std::string>;
using Config = std::unordered_map<std::string, Section>;

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

void parseConfig(Config &config)
{
  // NOTE: hardcoded config.ini path
  std::ifstream file("../config.ini");
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