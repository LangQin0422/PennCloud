#include <fstream>
#include <sstream>
#include <random>
#include <numeric>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <curl/curl.h>
#include <chrono>

// Write callback function to handle the data received from server
size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realSize = size * nmemb;
  *static_cast<std::atomic<size_t> *>(userp) += realSize; // Increment the size counter by the data received
  return realSize;
}

// Function to send HTTP request and track response payload size
void sendRequest(const std::vector<std::string> &urls, std::atomic<int> &successCount, std::vector<double> &durations, int index, std::atomic<size_t> &totalPayloadSize)
{
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> distrib(0, urls.size() - 1);

  CURL *curl = curl_easy_init();
  if (curl)
  {
    int urlIndex = distrib(gen);
    std::string url = urls[urlIndex];

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &totalPayloadSize); // Pass the size counter to the callback
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    auto start = std::chrono::high_resolution_clock::now();
    CURLcode res = curl_easy_perform(curl);
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> response_time = end - start;
    durations[index] = response_time.count();

    if (res == CURLE_OK)
    {
      long response_code;
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
      if (response_code == 200)
      {
        successCount++;
      }
    }
    curl_easy_cleanup(curl);
  }
  else
  {
    durations[index] = 0.0;
  }
}

// Stress test function
std::string stressTest(const std::vector<std::string> &urls, int numRequests)
{
  std::vector<std::thread> threads;
  std::vector<double> durations(numRequests, 0.0);
  std::atomic<int> successCount(0);
  std::atomic<size_t> totalPayloadSize(0);

  auto testStart = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < numRequests; ++i)
  {
    threads.emplace_back(sendRequest, std::ref(urls), std::ref(successCount), std::ref(durations), i, std::ref(totalPayloadSize));
  }

  for (auto &thread : threads)
  {
    thread.join();
  }

  auto testEnd = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> testDuration = testEnd - testStart;
  double totalDuration = std::accumulate(durations.begin(), durations.end(), 0.0);
  double averageDuration = totalDuration / numRequests;
  double seconds = testDuration.count() / 1000.0;
  double throughput = numRequests / seconds;
  double averagePayload = static_cast<double>(totalPayloadSize) / successCount / 1024;

  std::cout << "Total requests: " << numRequests << std::endl;
  std::cout << "200 OK responses: " << successCount << std::endl;
  std::cout << "Average response time: " << averageDuration << " ms" << std::endl;
  std::cout << "Throughput: " << throughput << " requests per second" << std::endl;
  std::cout << "Average payload size: " << averagePayload << " kb" << std::endl;

  std::cout << "------------------------------------" << std::endl;

  std::stringstream json;
  json << "{";
  json << "\"num_requests\": " << numRequests << ", ";
  json << "\"200_ok_responses\": " << successCount << ", ";
  json << "\"average_response_time_ms\": " << averageDuration << ", ";
  json << "\"throughput_requests_per_second\": " << throughput << ", ";
  json << "\"average_payload_kb\": " << averagePayload;
  json << "}";

  return json.str();
}

std::vector<std::string> testUrls()
{
  std::vector<std::string> urls;
  if (1 != 1)
  {
    urls = {
        "127.0.0.1:8000/",
        "127.0.0.1:8000/admin",
        "127.0.0.1:8000/kvs",
        "127.0.0.1:8000/api/workers",
        "127.0.0.1:8000/api/kvs",
        "127.0.0.1:8000/api/kvs/viewRows",
    };
  }
  else
  {
    urls = {
        "127.0.0.1:8001/",
        "127.0.0.1:8002/",
        "127.0.0.1:8003/",
        "127.0.0.1:8001/pages/home.html",
        "127.0.0.1:8002/pages/home.html",
        "127.0.0.1:8003/pages/home.html",
        "127.0.0.1:8001/pages/mail.html",
        "127.0.0.1:8002/pages/mail.html",
        "127.0.0.1:8003/pages/mail.html",
        "127.0.0.1:8001/pages/storage.html",
        "127.0.0.1:8002/pages/storage.html",
        "127.0.0.1:8003/pages/storage.html",
    };
  }
  return urls;
}

int main(int argc, char *argv[])
{
  std::vector<std::string> urls = testUrls();
  std::vector<int>
      requestLevels = {10, 100, 1000, 2000, 5000, 10000, 15000, 20000};
  std::vector<std::string> results;

  curl_global_init(CURL_GLOBAL_ALL);
  for (int numRequests : requestLevels)
  {
    std::string result = stressTest(urls, numRequests);
    results.push_back(result);
  }
  curl_global_cleanup();

  // Save all results to a file
  std::ofstream outFile("results.json");
  outFile << "[";
  for (size_t i = 0; i < results.size(); ++i)
  {
    if (i > 0)
      outFile << ", ";
    outFile << results[i];
  }
  outFile << "]";
  outFile.close();

  return 0;
}
