#include <string>

std::string extractDomain(const std::string &email);
std::string lookupMailServer(const std::string &domain, int verbose);
int connectToMailServer(const std::string &serverAddress, int verbose);
bool sendSMTPCommand(int sock, const std::string &command, const std::string &expectedResponse, int verbose);
bool sendEmail(int sock, const std::string &from, const std::string &to, const std::string &emailContent, int verbose);
void extract_mqueue(const int &fd, std::vector<std::string> &messages);
