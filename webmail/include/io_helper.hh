#include <vector>
#include <string>

std::string extract_username(const std::string &filename);
std::vector<std::string> extract_users(const std::string &file_path);
std::vector<std::string> splitMessages(const std::string &mboxContent, bool full = false);
