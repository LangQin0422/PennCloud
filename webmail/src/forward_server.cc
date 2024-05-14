#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <getopt.h>
#include <errno.h>
#include <algorithm>
#include <vector>
#include <cstring>
#include <set>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <set>
#include <cstdio>
#include <iostream>

#include "../include/MX_helper.hh"

int verbose = 0; // Verbose output

int main(int argc, char *argv[])
{
    int option;
    char *file_path = NULL;

    // Parse command-line options
    while ((option = getopt(argc, argv, ":av")) != -1)
    {
        switch (option)
        {
        case 'a':
            fprintf(stderr, "Author: Zhengyi Xiao / zxiao98\n");
            exit(EXIT_SUCCESS);
        case 'v':
            verbose = 1;
            break;
        default:
            fprintf(stderr, "Usage: %s [-a] [-v] <file_path>\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (optind < argc)
    {
        file_path = argv[optind];
    }
    else
    {
        fprintf(stderr, "Expected file path after options\n");
        exit(EXIT_FAILURE);
    }

    char *mboxPath = (char *)malloc(strlen(file_path) + 8);
    strcpy(mboxPath, file_path);
    strcat(mboxPath, "/mqueue");

    while (true) 
    {
      int fd = open(mboxPath, O_RDWR);
      if (fd == -1) {
          fprintf(stderr, "Failed to open mbox file\n");
          free(mboxPath);
          return -1;
      }

      // Acquiring lock on the file
      if (flock(fd, LOCK_EX) == -1) {
          fprintf(stderr, "Failed to lock mbox file\n");
          close(fd);
          free(mboxPath);
          return -1;
      }

      std::vector<std::string> messages;
      std::set<std::string> notSentMessages;

      // extract the messages from the mbox file
      extract_mqueue(fd, messages);

      if (verbose) {
          fprintf(stderr, "S: Messages to be sent: %lu\n", messages.size());
      }

      // iterate through the messages and send them
      for (const auto &message : messages)
      {
          // get the first \n in the message
          size_t firstNewline = message.find("\n");
          std::string input = message.substr(0, firstNewline);

          // extract the sender and recipient
          size_t startPos = input.find('<');
          size_t endPos = input.find('>', startPos);
          std::string sender = input.substr(startPos + 1, endPos - startPos - 1);

          startPos = input.find('<', endPos);
          endPos = input.find('>', startPos);
          std::string recipient = input.substr(startPos + 1, endPos - startPos - 1);

          // extract the body
          std::string body = message.substr(firstNewline + 1);

          // extract the domain and mail server
          std::string domain = extractDomain(recipient);
          std::string serverAddress = lookupMailServer(domain, verbose);

          // connect to the mail server
          int sock = connectToMailServer(serverAddress, verbose);

          if (sock >= 0)
          {
              if (!sendEmail(sock, sender, recipient, body, verbose))
                  notSentMessages.insert(message);
              close(sock);
          }
      }

      if (verbose)
          fprintf(stderr, "S: There are %lu messages did not sent and will remain the mqueu.\n", notSentMessages.size());

      // write the messages that are not sent back to the mqueue
      ftruncate(fd, 0);
      lseek(fd, 0, SEEK_SET);

      for (const auto &message : notSentMessages)
      {
          write(fd, message.c_str(), message.length());
      }

      // Unlock the file
      flock(fd, LOCK_UN);
      close(fd);
      sleep(5);
    }

    free(mboxPath);
    return 0;
}
