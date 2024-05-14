#include "KVSClient.hpp"

extern KVSClient kvsClient;

/**
 * Splits a string into tokens using a delimiter.
 *
 * @param input The input string to split.
 * @param delimiter The delimiter character.
 * @return A vector containing the tokens.
 */
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