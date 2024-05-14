#include <unordered_map>
#include <string>

#include "KVSCTRLClient.hpp"
#include "KVSClient.hpp"

extern KVSCTRLClient kvsCtrlClient;
extern KVSClient kvsClient;

extern std::unordered_map<std::string, std::string> ipPortMap;