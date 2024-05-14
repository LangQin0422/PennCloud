void processMailFromCommand(int sock, const char *argument, char **reversePath, bool verbose);
void processRcptToCommand(int sock, const char *argument, std::vector<std::string> &forwardPaths, bool verbose, int extraCredit);
void processDataCommand(int sock, bool &isInDataMode, char *emailBuffer, size_t &emailBufferLength, std::vector<std::string> &forwardPaths, char *&reversePath, const std::string &file_path, char *buffer, bool verbose);
void writeEmailToMbox(const std::string &emailContent, const std::vector<std::string> &forwardPaths, const std::string &senderEmail, const std::string &file_path);
