#pragma once   // Include guard
#include <iostream>
#include <cstring>  // For memset
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> // For close
#include <request.hh>
#include <response.hh>
#include "loginService.hh"
#include <vector>
#include <algorithm>
#include <string>

bool debugging = true; // TODO: set to false when deploy

void send500Response(Response &response, const std::string &message) {
    response.status(500, "Internal Server Error");
    response.body(message);
    response.type("text/plain");
    response.flush();
}

std::string readWebmailResponse(int sock) {
    std::vector<char> response;
    size_t totalRead = 0;
    constexpr size_t bufferSize = 1024;
    char tempBuffer[bufferSize];
    int bytesRead;

    // Read response in chunks until the server closes the connection or sends a terminating sequence
    while ((bytesRead = read(sock, tempBuffer, bufferSize - 1)) > 0) {
        tempBuffer[bytesRead] = '\0';  // Null-terminate the string
        response.insert(response.end(), tempBuffer, tempBuffer + bytesRead);

        // Check for termination sequence (end of message)
        if (response.size() >= 2 
            && response[response.size() - 2] == '\r' && response[response.size() - 1] == '\n') {
            break;
        }
    }

    // Convert vector to string
    return std::string(response.begin(), response.end());
}

/**
 * a variant of the readWebmailResponse, terminate on '.'
*/
std::string readData(int sock) {
    std::vector<char> response;
    size_t totalRead = 0;
    constexpr size_t bufferSize = 1024;
    char tempBuffer[bufferSize];
    int bytesRead;

    // Read response in chunks until the server closes the connection or sends a terminating sequence
    while ((bytesRead = read(sock, tempBuffer, bufferSize - 1)) > 0) {
        tempBuffer[bytesRead] = '\0';  // Null-terminate the string
        response.insert(response.end(), tempBuffer, tempBuffer + bytesRead);

        // Check for POP3 termination sequence (end of message)
        if (response.size() >= 3 
            && response[response.size() - 3] == '.' && response[response.size() - 2] == '\r'
            && response[response.size() - 1] == '\n') {
            break;
        }
    }

    // Convert vector to string
    return std::string(response.begin(), response.end());
}

std::string escapeJSONValue(const std::string &input) {
    std::string output;
    for (char c : input) {
        switch (c) {
            case '\"':
                output += "\\\"";
                break;
            case '\\':
                output += "\\\\";
                break;
            case '\b':
                output += "\\b";
                break;
            case '\f':
                output += "\\f";
                break;
            case '\n':
                output += "\\n";
                break;
            case '\r':
                output += "\\r";
                break;
            case '\t':
                output += "\\t";
                break;
            default:
                // Append the character as is if it's not a control character.
                if (static_cast<unsigned char>(c) >= 0x20) {  // Skip control characters.
                    output += c;
                }
                break;
        }
    }
    return output;
}

/**
 * Convert an email in raw format to JSON format. The raw email format is as follows:
 * "+OK 66 octets
Dear professor linh:
This is a test email.

Best,
Ruikun Hao
.
"
*/
string emailToJSON(string raw, string messageId) {
    std::stringstream ss(raw);
    std::string line;
    std::string email = "{\"id\": \"" + messageId + "\", ";
    try {
        getline(ss, line); // skip the rest of the first line
        email += "\"body\": \"";
        while (getline(ss, line)) {
            // Remove \r if it exists
            if (line[line.length() - 1] == '\r') {
                line.pop_back();
            }
            if (line == ".") {
                break;
            }
            email += escapeJSONValue(line) + "\\n";
        }
        email += "\"";
        email += "}";

        return email;
    } catch (std::invalid_argument &e) {
        return "\"Invalid email format\"";
    }
}

/**
 * helper function to send a command to the POP3 server and check the response
 * serverResponse will contain the response from the server
 * 
 * @return true if the command was successful, false otherwise
*/
bool sendPOP3Command(const std::string& command, std::string& serverResponse, int sock) {
    if (send(sock, command.c_str(), command.length(), 0) < 0) {
        return false;  // Send failed
    }
    if (command == "QUIT\r\n") {
        return true;  // No need to check response for QUIT command
    }
    string prefix = command.substr(0, 4);
    if (prefix == "UIDL" || prefix == "RETR") {
        serverResponse = readData(sock);
    } else {
        serverResponse = readWebmailResponse(sock);
    }

    return serverResponse.substr(0, 3).compare("+OK") == 0;
};

void releaseLock(const std::string& rowKey, const std::string& mutexId) {
    if (!kvsClient.Del(rowKey, mutexId)) // relase the lock
    {
        std::cerr << "Del failed" << std::endl;
    } else if (debugging) {
        std::cout << "Lock release success, mutexId: " << mutexId  << std::endl;
    }
}

// note in this version, we don't need to go through pop3 to reduce overhead
void getMailbox(std::vector<std::pair<std::string, std::string>>& mailbox, const std::string& user)
{
  std::string rowKey = user + ".mbox";
  mailbox.clear();
  std::string mutexId = "-";
  bool mutexAcquired = false;
    do { // keep trying until mutex is acquired
      if (debugging) {
        std::cout << "Try locking " << rowKey << std::endl;
      }
      mutexAcquired = kvsClient.SetNX(rowKey, mutexId);
      if (!mutexAcquired) sleep(1);
    } while (!mutexAcquired);
  std::vector<std::string> colKeys;

  if (!kvsClient.GetColsInRow(rowKey, colKeys, mutexId))
  {
    std::cerr << "GetColsInRow failed" << std::endl;
    releaseLock(rowKey, mutexId);
    return;
  }
  
  for (const auto& colKey: colKeys)
  {
    // std::cout << "colKey: " << colKey << std::endl;
    std::string value;
    if (!kvsClient.Get(rowKey, colKey, value, mutexId))
    {
      std::cerr << "Get failed in getMailbox" << std::endl;
      releaseLock(rowKey, mutexId);
      return;
    }
    mailbox.emplace_back(colKey, value);
  }
  releaseLock(rowKey, mutexId);
}

void handleGetEmails(const Request &request, Response &response) {
    SessionData sessionData;
    if (loggedIn(request, sessionData)) {

        std::vector<std::pair<std::string, std::string>> messages;
        getMailbox(messages, sessionData.username);

        // Convert messages to JSON format
        std::string jsonResponse = "[";
        for (size_t i = 0; i < messages.size(); ++i) {
            jsonResponse += emailToJSON(messages[i].second, messages[i].first);
            if (i < messages.size() - 1) {
                jsonResponse += ", ";
            }
        }
        jsonResponse += "]";

        // Sending the response back to the client in JSON format
        response.status(200, "OK");
        response.body(jsonResponse);
        response.type("application/json");
        response.flush();
    } else {
        send500Response(response, "Unauthorized");
    }
}

void handleDeleteEmail(const Request &request, Response &response) {
    SessionData sessionData;
    if (loggedIn(request, sessionData)) {
        string id = request.params("id");
        std::string rowKey = sessionData.username + ".mbox";
        std::string mutexId = "-";
        bool mutexAcquired = false;
        do { // keep trying until mutex is acquired
            if (debugging) {
                std::cout << "Try locking " << rowKey << std::endl;
            }
            mutexAcquired = kvsClient.SetNX(rowKey, mutexId);
            if (!mutexAcquired) sleep(1);
        } while (!mutexAcquired);
        
        if (!kvsClient.Delete(rowKey, id, mutexId))
        {
            std::cerr << "DelColInRow failed" << std::endl;
            send500Response(response, "Internal server error: DelColInRow failed");
            releaseLock(rowKey, mutexId);
            return;
        } else if (debugging) {
            std::cout << "DelColInRow success in handleDeleteEmail, mutexId: " << mutexId  << std::endl;
        }

        if (!kvsClient.Del(rowKey, mutexId)) // relase the lock
        {
            std::cerr << "Del failed in getMailbox" << std::endl;
            send500Response(response, "Internal server error: Del failed in getMailbox");
        } else if (debugging) {
            std::cout << "Lock release success in getMailbox, mutexId: " << mutexId  << std::endl;
        }
        // Sending the response back to the client in JSON format
        response.status(200, "OK");
        response.body("Message with ID " + id + " deleted successfully");
        response.type("text/plain");
        response.flush();
    } else {
        send500Response(response, "Unauthorized");
    }
}

#if 0
/**
 * a helper function to parse a simple json string
*/
std::unordered_map<std::string, std::string> parseJson(const std::string& json) {
    std::unordered_map<std::string, std::string> result;
    std::string key, value;
    bool isKey = true;
    bool isString = false;

    for (char ch : json) {
        if (ch == '{' || ch == '}' || ch == ' ' || ch == ',') {
            continue;
        }

        if (ch == ':') {
            isKey = false;
            continue;
        }

        if (ch == '"') {
            isString = !isString;
            if (!isString) {
                if (isKey) {
                    key = value;
                } else {
                    result[key] = value;
                    isKey = true;
                }
                value.clear();
            }
            continue;
        }

        if (isString) {
            value += ch;
        }
    }

    return result;
}
#endif


/*
    SMTP section
*/ 

/**
 * helper function to send a command to the POP3 server and check the response
 * serverResponse will contain the response from the server
 * 
 * @return the status code of the response
*/
int sendSMTPCommand(const std::string& command, std::string& serverResponse, int sock) {
    if (send(sock, command.c_str(), command.length(), 0) < 0) {
        return -1;  // Send failed
    }
    serverResponse = readWebmailResponse(sock);
    return std::stoi(serverResponse.substr(0, 3));
};


void handleSendEmail(const Request &request, Response &response) {
    SessionData sessionData;
    if (loggedIn(request, sessionData)) {
        int sock = 0;
        struct sockaddr_in serv_addr;
        char buffer[1024] = {0};

        // Creating socket
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            std::cerr << "Socket creation error" << std::endl;
            send500Response(response, "Socket creation failed");
            return;
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(2500);

        // Convert IPv4 and IPv6 addresses from text to binary form
        if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
            std::cerr << "Invalid address/ Address not supported" << std::endl;
            close(sock);
            send500Response(response, "Invalid address");
            return;
        }

        // Establishing connection to the server
        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            std::cerr << "Connection Failed" << std::endl;
            close(sock);
            send500Response(response, "Connection failed");
            return;
        }

        string mailFrom = request.headers("from");
        string rcptTo = request.headers("to");

        std::string serverResponse = readWebmailResponse(sock);
        if (serverResponse.substr(0, 3).compare("220") != 0) {
            close(sock);
            send500Response(response, "SMTP Connection failed");
            return;
        }
        // Sending HELO command and checking response
        if (sendSMTPCommand("HELO penncloud07.com\r\n", serverResponse, sock) != 250) {
            close(sock);
            send500Response(response, "HELO command failed");
            return;
        }

        // Sending MAIL FROM command and checking response
        if (sendSMTPCommand("MAIL FROM:<" + mailFrom + ">\r\n", serverResponse, sock) != 250) {
            close(sock);
            send500Response(response, "MAIL FROM command failed");
            return;
        }
        std::stringstream ss(rcptTo);
        std::string to;
        while (ss >> to) {
            // Sending RCPT TO command and checking response
            if (sendSMTPCommand("RCPT TO:<" + to + ">\r\n", serverResponse, sock) != 250) {
                close(sock);
                send500Response(response, "RCPT TO command failed");
                return;
            }
        }

        // Sending DATA command and checking response
        if (sendSMTPCommand("DATA\r\n", serverResponse, sock) != 354) {
            close(sock);
            send500Response(response, "DATA command failed");
            return;
        }

        // Sending the email content
        std::string emailContent = request.body();
        if (sendSMTPCommand(emailContent + "\r\n.\r\n", serverResponse, sock) != 250) {
            close(sock);
            send500Response(response, "Email content sending failed");
            return;
        }

        // At this point, message is successfully sent
        // Closing the socket
        sendSMTPCommand("QUIT\r\n", serverResponse, sock); // No need to check response for QUIT command
        close(sock);

        // Sending the response back to the client in JSON format
        response.status(200, "OK");
        response.body("Email sent successfully");
        response.type("text/plain");
        response.flush();
    } else {
        send500Response(response, "Unauthorized");
    }
}