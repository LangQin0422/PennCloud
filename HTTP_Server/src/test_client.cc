#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <curl/curl.h>
#include <vector>

// Function to read the file content into a string
std::string readFile(const std::string &fileName)
{
  std::ifstream file(fileName, std::ios::binary | std::ios::ate);
  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<char> buffer(size);
  if (file.read(buffer.data(), size))
  {
    /* Successfully read file into buffer */
    return std::string(buffer.begin(), buffer.end());
  }
  /* Handle error */
  return "";
}

size_t writeCallback(void *contents, size_t size, size_t nmemb, std::string *s)
{
  size_t newLength = size * nmemb;
  try
  {
    s->append((char *)contents, newLength);
  }
  catch (std::bad_alloc &e)
  {
    // handle memory problem
    return 0;
  }
  return newLength;
}

void sendPostRequest(const std::string &url, const std::string &data)
{
  CURL *curl = curl_easy_init();
  std::string responseString;
  if (curl)
  {
    // Set the URL for the POST request
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    // Create a list of headers
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Set the POST data
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, data.size());

    // Set up response capture
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseString);

    // Perform the request, res will get the return code
    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK)
    {
      std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
    }
    else
    {
      // Compare the response with the original message
      if (responseString == data)
      {
        std::cout << "Success: The response matches the sent message." << std::endl;
      }
      else
      {
        std::cout << "Error: The response does not match the sent message." << std::endl;
      }
    }

    // Cleanup
    curl_slist_free_all(headers); // Free the header list
    curl_easy_cleanup(curl);
  }
}

int main(int argc, char *argv[])
{
  if (argc != 3)
  {
    std::cout << "Usage: " << argv[0] << " <file path> <URL>" << std::endl;
    return 1;
  }

  std::string filePath = argv[1];
  std::string url = argv[2];
  std::string query_url = url + "?test=123&cookie=321";

  std::string fileContent = readFile(filePath);
  sendPostRequest(query_url, fileContent);

  return 0;
}
