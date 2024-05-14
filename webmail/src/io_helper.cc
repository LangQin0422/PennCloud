#include <iostream>
#include <vector>
#include <string>
#include <dirent.h>
#include <iostream>
#include <sstream>

// Function to extract the username from a mailbox filename
std::string extract_username(const std::string &filename)
{
  size_t lastDot = filename.rfind(".mbox");
  if (lastDot != std::string::npos)
  {
    return filename.substr(0, lastDot);
  }
  return filename; // Return the original filename if ".mbox" not found
}

std::vector<std::string> extract_users(const std::string &file_path)
{
  std::vector<std::string> users;
  DIR *dir;
  struct dirent *entry;

  dir = opendir(file_path.c_str());
  if (!dir)
  {
    std::cerr << "opendir() error with path: " << file_path << std::endl;
    return users; // Return empty vector in case of error
  }

  while ((entry = readdir(dir)) != nullptr)
  {
    std::string filename(entry->d_name);
    // Ensure we process only .mbox files
    if (filename.find(".mbox") != std::string::npos)
    {
      users.push_back(extract_username(filename));
    }
  }

  closedir(dir);
  return users;
}

/**
 * @brief Splits the contents of an mbox file into individual email messages based on the "From " line delimiter.
 *        The function iterates through each line of the mbox content, grouping lines into messages. A new message
 *        is started each time a line beginning with "From " is encountered. This mimics the way email clients and
 *        servers process mbox files, where each email is preceded by a line starting with "From ".
 *        The function offers an option to include or exclude the "From " delimiter line in the returned messages
 *        through the `full` parameter.
 *
 * @param mboxContent A string containing the entire content of an mbox file.
 * @param full A boolean flag indicating whether to include the "From " delimiter line at the start of each message.
 *             When true, each message in the returned vector includes its "From " line.
 *             When false, the "From " lines are excluded from the message contents.
 * @return std::vector<std::string> A vector of strings, where each string represents an individual email message
 *         extracted from the mbox content. The inclusion of "From " lines in these messages is controlled by the `full` parameter.
 */
std::vector<std::string> splitMessages(const std::string &mboxContent, bool full = false)
{

  std::vector<std::string> lines;
  std::istringstream stream(mboxContent);
  std::string line;

  // Split mboxContent into lines
  while (std::getline(stream, line))
  {
    lines.push_back(line);
  }

  std::vector<std::string> messages;
  std::string currentMessage;

  for (size_t i = 0; i < lines.size(); ++i)
  {
    // Check if the line starts with "From "
    if (lines[i].substr(0, 5) == "From ")
    {
      if (!currentMessage.empty())
      {
        // If there's a current message being built, push it to messages and start a new one
        messages.push_back(currentMessage);
        currentMessage.clear();
      }
      if (full) // include .mbox line "From ...<LF>"
      {
        currentMessage += lines[i] + "\n";
      }
    }
    else
    {
      // If the line doesn't start with "From ", append it to the current message
      currentMessage += lines[i] + "\n";
    }
  }

  // Add the last message if it exists
  if (!currentMessage.empty())
  {
    messages.push_back(currentMessage);
  }

  return messages;
}