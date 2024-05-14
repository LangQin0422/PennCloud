#include <vector>
#include <string>

void processUserCommand(int sock, char *argument, char **username, bool verbose);
void processPassCommand(int sock, char *argument, bool *loggedIn, char **user, bool verbose, std::string& mutexId);
void processStatCommand(int sock, char* user, std::set<unsigned int> &deletedMessage, bool verbose, const std::string& mutexId);
void processListCommand(int sock, char* user, char *argument, std::set<unsigned int> &deletedMessage, bool verbose, const std::string& mutexId);
void processUidlCommand(int sock, char* user, char *argument, std::set<unsigned int> &deletedMessage, bool verbose, const std::string& mutexId);
void processRetrCommand(int sock, char* user, char *argument, std::set<unsigned int> &deletedMessages, bool verbose, const std::string& mutexId);
void processDeleCommand(int sock, char* user, char *argument, std::set<unsigned int> &deletedMessages, bool verbose, const std::string& mutexId);
void processRsetCommand(int sock, char* user, std::set<unsigned int> &deletedMessages, bool verbose, const std::string& mutexId);
void processQuitCommand(int sock, char* user, std::set<unsigned int> &deletedMessages, bool verbose, std::string& mutexId);

void computeDigest(char *data, int dataLengthBytes, unsigned char *digestBuffer);
