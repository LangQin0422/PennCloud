#include <arpa/inet.h>
#include <filesystem>
#include <helper.hh>
#include <iomanip>
#include "loginService.hh"
#include <request.hh>
#include <response.hh>
#include <unistd.h>

std::string formatBytes(long long bytes)
{
    const long long K = 1024;
    const long long M = K * K;
    const long long G = M * K;
    const long long T = G * K;

    std::stringstream ss;
    ss << std::fixed << std::setprecision(2); // Set precision to 2 decimal places

    if (bytes >= T) {
        ss << (double)bytes / T << "TB";
    } else if (bytes >= G) {
        ss << (double)bytes / G << "GB";
    } else if (bytes >= M) {
        ss << (double)bytes / M << "MB";
    } else if (bytes >= K) {
        ss << (double)bytes / K << "KB";
    } else {
        ss << bytes << "B";
    }

    return ss.str();
}

bool hasFile(std::string fileIdx, std::string fileName)
{
    // Check if file name already existed
    std::istringstream fileIdxStream(fileIdx);
    std::string line;
    while (std::getline(fileIdxStream, line))
    {
        std::istringstream lineStream(line);
        std::string name;
        lineStream >> name;
        if (name == fileName)
        {
            return true;
        }
    }

    return false;
}

bool hasFolder(std::string folderIdx, std::string folderName)
{
    // Check if folder name already existed
    std::istringstream folderIdxStream(folderIdx);
    std::string line;

    // Skips the first line which is the current folder's path
    std::getline(folderIdxStream, line);
    while (std::getline(folderIdxStream, line))
    {
        if (line == folderName)
        {
            return true;
        }
    }

    return false;
}

void deleteFolder(std::string userR)
{
    // Check for any subfolders in the folder
    std::string folderIdx;
    if (kvsClient.Get(userR, "folderIndex.txt", folderIdx))
    {
        std::istringstream folderIdxStream(folderIdx);
        std::string line;

        // Skips the first line whcih is the current folder's path
        std::getline(folderIdxStream, line);
        while (std::getline(folderIdxStream, line))
        {
            if (line != "..")
            {
                // Recursively delete all subfolders
                std::string folderPath = userR + "/" + line;
                deleteFolder(folderPath);
            }
        }

        // After deleting all subfolders, delete the folder index
        kvsClient.Delete(userR, "folderIndex.txt");
    }

    // Check for any files in the folder
    std::string fileIdx;
    if (kvsClient.Get(userR, "fileIndex.txt", fileIdx))
    {
        std::istringstream fileIdxStream(fileIdx);
        std::string line;
        while (std::getline(fileIdxStream, line))
        {
            std::istringstream lineStream(line);
            std::string fileName;
            lineStream >> fileName;
            kvsClient.Delete(userR, fileName);
        }

        // After deleting all files, delete the file index
        kvsClient.Delete(userR, "fileIndex.txt");
    }
}

void moveFolder(const std::string &oldUserR, const std::string &newUserR, const std::string &newFolderPath)
{
    // Check for any subfolders in the folder
    std::string folderIdx;
    if (kvsClient.Get(oldUserR, "folderIndex.txt", folderIdx))
    {
        std::istringstream folderIdxStream(folderIdx);
        std::string line;

        std::ostringstream newFolderIdxStream;
        newFolderIdxStream << newFolderPath << "\n";

        // Skips the first line whcih is the current folder's path
        std::getline(folderIdxStream, line);
        while (std::getline(folderIdxStream, line))
        {
            if (line != "..")
            {
                // Recursively delete all subfolders
                std::string oldFolderR = oldUserR + "/" + line;
                std::string newFolderR = newUserR + "/" + line;
                moveFolder(oldFolderR, newFolderR, newFolderPath + "/" + line);
            }

            newFolderIdxStream << line << "\n";
        }

        // Create a folder index for the new folder, and delete the folder index for the old folder
        kvsClient.Put(newUserR, "folderIndex.txt", newFolderIdxStream.str());
        kvsClient.Delete(oldUserR, "folderIndex.txt");
    }

    // Check for any files in the folder
    std::string fileIdx;
    if (kvsClient.Get(oldUserR, "fileIndex.txt", fileIdx))
    {
        std::istringstream fileIdxStream(fileIdx);
        std::string line;

        std::ostringstream newfileIdxStream;

        while (std::getline(fileIdxStream, line))
        {
            // Copy the file info to the new file index
            newfileIdxStream << line << "\n";

            // Get the name of file to copy
            std::istringstream lineStream(line);
            std::string fileName;
            lineStream >> fileName;

            // Copy the file to the new folder
            std::string fileContent;
            kvsClient.Get(oldUserR, fileName, fileContent);
            kvsClient.Put(newUserR, fileName, fileContent);
            kvsClient.Delete(oldUserR, fileName);
        }

        // Create a file index for the new folder, and delete the file index for the old folder
        kvsClient.Put(newUserR, "fileIndex.txt", newfileIdxStream.str());
        kvsClient.Delete(oldUserR, "fileIndex.txt");
    }
}

void handleGetFiles(const Request &request, Response &response)
{
    try {
        // If the user is not logged in, set an error response
        SessionData sessionData;
        if (!loggedIn(request, sessionData)) 
        {
            response.status(500, "Internal Server Error");
            response.body("Unauthorized");
            response.flush();
            return;
        }

        std::string folderPath = request.queryParams("path") == "/" ? "" : request.queryParams("path");

        // check if the user's row exists, if false, create a new row with the user's username
        std::string userR = "./webstorage/" + sessionData.username + "/home" + folderPath;
        std::string fileIdx;
        if (!kvsClient.Get(userR, "fileIndex.txt", fileIdx))
        {
            kvsClient.Put(userR, "fileIndex.txt", "");
            response.status(204, "OK");
            response.flush();
            return;
        }

        // Iterate through the fileIndex.txt and get the file info
        std::istringstream fileIndexStream(fileIdx);
        std::string line, jsonResponse = "[";
        while (std::getline(fileIndexStream, line))
        {
            // parse the line into file name, size, and date
            std::istringstream lineStream(line);
            std::string name, size, type, date, time, clock;
            lineStream >> name >> size >> type >> date >> time >> clock;

            // format date time
            std::string dateTime = date + " " + time + " " + clock;

            // convert to json
            jsonResponse += "{\"name\": \"" + name + "\", \"size\": \"" + size + "\", \"type\": \"" + type + "\", \"date\": \"" + dateTime + "\"},";
        }
        jsonResponse.pop_back(); // remove the last comma
        jsonResponse += "]";

        if (jsonResponse != "]")
        {
            response.status(200, "OK");
            response.type("application/json");
            response.body(jsonResponse);
        }
        else
        {
            response.status(204, "OK");
        }

        // Flush the response
        response.flush();
    } catch (std::exception &e) {
        response.status(500, "Internal Server Error");
        response.body("500 Internal Server Error: Exception- " + std::string(e.what()));
        response.flush();
    }
}

void handleGetFile(const Request &request, Response &response)
{
    if (true)
    {
        std::cout << "Download file" << std::endl;
    }

    try {
        // If the user is not logged in, set an error response
        SessionData sessionData;
        if (!loggedIn(request, sessionData)) 
        {
            response.status(500, "Internal Server Error");
            response.body("Unauthorized");
            response.flush();
            return;
        }

        if (true)
        {
            std::cout << "Get row key" << std::endl;
        }

        std::string folderPath = request.queryParams("path") == "/" ? "" : request.queryParams("path");

        if (true)
        {
            std::cout << "Get file name" << std::endl;
        }

        // Get the file name from the request
        std::string fileName = request.params("name");

        if (true)
        {
            std::cout << "folderPath: " << folderPath << std::endl;
            std::cout << "File name: " << fileName << std::endl;
            std::cout << "Get file index" << std::endl;
        }

        // Get the file index and find the file type of the file
        std::string userR = "./webstorage/" + sessionData.username + "/home" + folderPath;
        std::string fileIdx;
        if (!kvsClient.Get(userR, "fileIndex.txt", fileIdx))
        {
            response.status(404, "Not Found");
            response.body("404 Not Found: File index not found");
            response.flush();
            return;
        }

        if (true)
        {
            std::cout << "Get file type" << std::endl;
        }

        // Get the file type
        std::istringstream fileIdxStream(fileIdx);
        std::string line;
        std::string mimeType;
        while (std::getline(fileIdxStream, line))
        {
            std::istringstream lineStream(line);
            std::string name, size, type;
            lineStream >> name >> size >> type;
            if (name == fileName)
            {
                mimeType = type;
                break;
            }
        }

        if (true)
        {
            std::cout << "Get file" << std::endl;
        }

        // Get the file
        std::string fileContent;
        if (kvsClient.Get(userR, fileName, fileContent))
        {
            // Set the response headers and body to the content of the file
            response.body(fileContent);
            response.type(mimeType);
            response.status(200, "OK");
        }
        else
        {
            // If the file cannot be opened, set an error response
            response.status(404, "Not Found");
            response.body("404 Not Found: File not found");
            std::cout << "File not found" << std::endl;
        }

        if (true)
        {
            std::cout << "Size of file: " << fileContent.size() << std::endl << std::endl;
        }
        
        // Flush the response
        response.flush();

        if (true)
        {
            std::cout << "Send download file resposne" << std::endl;
        }
    } catch (std::exception &e) {
        response.status(500, "Internal Server Error");
        response.body("500 Internal Server Error: Exception- " + std::string(e.what()));
        response.flush();
    }
}

void handleUploadFile(const Request &request, Response &response)
{
    if (true)
    {
        std::cout << "Uploading file" << std::endl;
    }
    
    try {
        // If the user is not logged in, set an error response
        SessionData sessionData;
        if (!loggedIn(request, sessionData)) 
        {
            response.status(500, "Internal Server Error");
            response.body("Unauthorized");
            response.flush();
            return;
        }

        if (true)
        {
            std::cout << "Get file info from route" << std::endl;
        }
        
        // Get the file name, size, and date from the request headers
        std::string fileName = request.headers("X-File-Name");
        std::string fileDate = request.headers("X-File-Last-Modified");
        std::string fileType = request.headers("X-File-Type");
        std::string fileSize = formatBytes(stoll(request.headers("X-File-Size")));
        std::string fileContent = request.body();
        std::string folderPath = request.queryParams("path") == "/" ? "" : request.queryParams("path");

        if (true)
        {
            std::cout << "File name: " << fileName << std::endl;
            std::cout << "File size: " << fileSize << std::endl;
            std::cout << "File type: " << fileType << std::endl;
            std::cout << "File date: " << fileDate << std::endl;
            std::cout << "File path: " << folderPath << std::endl;
            std::cout << "Size of file: " << fileContent.size() << std::endl;
            std::cout << "Get file index" << std::endl;
        }

        // Get the current files in the user's row
        std::string userR = "./webstorage/" + sessionData.username + "/home" + folderPath;
        std::string fileIdx;
        if (!kvsClient.Get(userR, "fileIndex.txt", fileIdx))
        {
            kvsClient.Put(userR, "fileIndex.txt", "");
            fileIdx = "";
        }

        if (true)
        {
            std::cout << "Check if file existed" << std::endl;
        }

        // Check if file name already existed
        if (hasFile(fileIdx, fileName))
        {
            response.status(500, "Internal Server Error");
            response.body("500 Internal Server Error: File already exists");
            response.flush();
            return;
        }

        if (true)
        {
            std::cout << "Save file content" << std::endl;
        }

        if (!kvsClient.Put(userR, fileName, fileContent))
        {
            // If the file cannot be opened, set an error response
            response.status(500, "Internal Server Error");
            response.body("500 Internal Server Error: File could not be saved");
            response.flush();
            return;
        }

        if (true)
        {
            std::cout << "Update file index" << std::endl;
        }

        // Save the file content in KVS and append the new file info to the file index 
        std::string newFileIdx = fileIdx + fileName + " " + fileSize + " " + fileType + " " + fileDate + "\n";
        if (kvsClient.CPut(userR, "fileIndex.txt", fileIdx, newFileIdx))
        {
            // create a json response containing the uploaded files info
            std::string jsonResponse = "{\"name\": \"" + fileName + "\", \"size\": \"" + fileSize + "\", \"type\": \"" + fileType + "\", \"date\": \"" + fileDate + "\"}";

            // Set the response status
            response.status(200, "OK");
            response.type("application/json");
            response.body(jsonResponse);
        }
        else
        {
            // If the file cannot be opened, set an error response
            response.status(500, "Internal Server Error");
            response.body("500 Internal Server Error: File Index could not be saved");
        }

        if (true)
        {
            std::cout << "Send upload file response" << std::endl;
        }

        // Flush the response
        response.flush();
    } catch (std::exception &e) {
        response.status(500, "Internal Server Error");
        response.body("500 Internal Server Error: Exception- " + std::string(e.what()));
        response.flush();
    }
}

void handleDeleteFile(const Request &request, Response &response)
{
    try {
        // If the user is not logged in, set an error response
        SessionData sessionData;
        if (!loggedIn(request, sessionData)) 
        {
            response.status(500, "Internal Server Error");
            response.body("Unauthorized");
            response.flush();
            return;
        }
        
        std::string fileName = request.params("name");
        std::string folderPath = request.queryParams("path") == "/" ? "" : request.queryParams("path");

        std::string userR = "./webstorage/" + sessionData.username + "/home" + folderPath;
        std::string fileIdx;
        if (!kvsClient.Get(userR, "fileIndex.txt", fileIdx) || !hasFile(fileIdx, fileName))
        {
            response.status(404, "Not Found");
            response.body("404 Not Found: File index not found");
            response.flush();
            return;
        }

        // remove file from kvs
        if (!kvsClient.Delete(userR, fileName))
        {
            response.status(500, "Internal Server Error");
            response.body("500 Internal Server Error: File could not be removed");
            response.flush();
            return;
        }

        // open file index and create temp file for writing new index
        std::istringstream fileIdxStream(fileIdx);
        std::string newFileIdx, line;
        while (std::getline(fileIdxStream, line))
        {
            if (line.find(fileName) == std::string::npos)
            {
                newFileIdx += line + "\n";
            }
        }

        // update file index
        if (!kvsClient.CPut(userR, "fileIndex.txt", fileIdx, newFileIdx))
        {
            response.status(500, "Internal Server Error");
            response.body("500 Internal Server Error: File index could not be updated");
            response.flush();
            return;
        }

        // Set the response status
        response.status(200, "OK");
        response.flush();
    } catch (std::exception &e) {
        response.status(500, "Internal Server Error");
        response.body("500 Internal Server Error: Exception- " + std::string(e.what()));
        response.flush();
    }
}

void handleMoveFile(const Request &request, Response &response)
{
    try {
        // If the user is not logged in, set an error response
        SessionData sessionData;
        if (!loggedIn(request, sessionData)) 
        {
            response.status(500, "Internal Server Error");
            response.body("Unauthorized");
            response.flush();
            return;
        }

        // Get the file to move and the destination folder
        std::string folderPath = request.queryParams("path") == "/" ? "" : request.queryParams("path");
        std::string fileName = request.params("name");
        std::string destPath = request.queryParams("dest") == "/" ? "" : request.queryParams("dest");

        // Check if the destination folder is the current folder
        if (folderPath == destPath)
        {
            response.status(500, "Internal Server Error");
            response.body("500 Internal Server Error: Cannot move file to current folder");
            response.flush();
            return;
        }

        // Get the file name and its new row key
        std::string oldFileR = "./webstorage/" + sessionData.username + "/home" + folderPath;
        std::string newFileR = "./webstorage/" + sessionData.username + "/home" + destPath;

        // Get the current files in old file row
        std::string oldFileRfileIdx;
        if (!kvsClient.Get(oldFileR, "fileIndex.txt", oldFileRfileIdx))
        {
            response.status(404, "Not Found");
            response.body("404 Not Found: File index not found");
            response.flush();
            return;
        }

        // Check if the file to move exists
        if (!hasFile(oldFileRfileIdx, fileName))
        {
            response.status(404, "Not Found");
            response.body("404 Not Found: File not found");
            response.flush();
            return;
        }

        // Get the curent files in new file row
        std::string newFileRFileIdx;
        if (!kvsClient.Get(newFileR, "fileIndex.txt", newFileRFileIdx))
        {
            response.status(404, "Not Found");
            response.body("404 Not Found: Move to File index not found");
            response.flush();
            return;
        }

        // Check if file name already existed in the destination folder
        if (hasFile(newFileRFileIdx, fileName))
        {
            response.status(500, "Internal Server Error");
            response.body("500 Internal Server Error: File already exists");
            response.flush();
            return;
        }

        // Move the file to the destination folder
        std::string fileContent;
        if (!kvsClient.Get(oldFileR, fileName, fileContent))
        {
            response.status(500, "Internal Server Error");
            response.body("500 Internal Server Error: File could not be opened");
            response.flush();
            return;
        }

        if (!kvsClient.Put(newFileR, fileName, fileContent))
        {
            response.status(500, "Internal Server Error");
            response.body("500 Internal Server Error: File could not be saved");
            response.flush();
            return;
        }

        // Remove the old file from old file R
        if (!kvsClient.Delete(oldFileR, fileName))
        {
            response.status(500, "Internal Server Error");
            response.body("500 Internal Server Error: File could not be removed");
            response.flush();
            return;
        }

        // Update old file R file index
        std::istringstream oldFileRfileIdxStream(oldFileRfileIdx);
        std::string newOldFileRfileIdx, line, fileInfo;
        while (std::getline(oldFileRfileIdxStream, line))
        {
            if (line.find(fileName) == std::string::npos)
            {
                newOldFileRfileIdx += line + "\n";
            }
            else
            {
                fileInfo = line;
            }
        }

        if (!kvsClient.CPut(oldFileR, "fileIndex.txt", oldFileRfileIdx, newOldFileRfileIdx))
        {
            response.status(500, "Internal Server Error");
            response.body("500 Internal Server Error: File index could not be updated");
            response.flush();
            return;
        }

        // Update new file R file index
        std::string newFileRfileIdxUpdate = newFileRFileIdx + fileInfo + "\n";
        if (!kvsClient.CPut(newFileR, "fileIndex.txt", newFileRFileIdx, newFileRfileIdxUpdate))
        {
            response.status(500, "Internal Server Error");
            response.body("500 Internal Server Error: File index could not be updated");
            response.flush();
            return;
        }

        // Set the response status
        response.status(200, "OK");
        response.flush();
    } catch (std::exception &e) {
        response.status(500, "Internal Server Error");
        response.body("500 Internal Server Error: Exception- " + std::string(e.what()));
        response.flush();
    }
}

void handleRenameFile(const Request &request, Response &response)
{
    try {
        // If the user is not logged in, set an error response
        SessionData sessionData;
        if (!loggedIn(request, sessionData)) 
        {
            response.status(500, "Internal Server Error");
            response.body("Unauthorized");
            response.flush();
            return;
        }
        
        std::string folderPath = request.queryParams("path") == "/" ? "" : request.queryParams("path");
        std::string fileName = request.params("name");
        std::string newFileName = request.params("newName");

        if (fileName == newFileName)
        {
            response.status(200, "OK");
            response.flush();
            return;
        }

        // Get the current files in the user's row
        std::string userR = "./webstorage/" + sessionData.username + "/home" + folderPath;
        std::string fileIdx;
        if (!kvsClient.Get(userR, "fileIndex.txt", fileIdx))
        {
            response.status(404, "Not Found");
            response.body("404 Not Found: File index not found");
            response.flush();
            return;
        }

        // Check if file name existed
        if (!hasFile(fileIdx, fileName))
        {
            response.status(404, "Not Found");
            response.body("404 Not Found: File not found");
            response.flush();
            return;
        }

        // Check if new file name existed
        if (hasFile(fileIdx, newFileName))
        {
            response.status(500, "Internal Server Error");
            response.body("500 Internal Server Error: File already exists");
            response.flush();
            return;
        }

        // Get the file contents
        std::string fileContent;
        if (!kvsClient.Get(userR, fileName, fileContent))
        {
            response.status(500, "Internal Server Error");
            response.body("500 Internal Server Error: File could not be opened");
            response.flush();
            return;
        }

        // Save the file content in KVS with new file name
        if (!kvsClient.Put(userR, newFileName, fileContent))
        {
            response.status(500, "Internal Server Error");
            response.body("500 Internal Server Error: File could not be saved");
            response.flush();
            return;
        }    

        // Remove the old file from KVS
        if (!kvsClient.Delete(userR, fileName))
        {
            response.status(500, "Internal Server Error");
            response.body("500 Internal Server Error: File could not be removed");
            response.flush();
            return;
        }

        // Open the file index and create a temp file for writing the new index
        std::istringstream fileIdxStream(fileIdx);
        std::string newFileIdx, line;
        while (std::getline(fileIdxStream, line))
        {
            if (line.find(fileName) == std::string::npos)
            {
                newFileIdx += line + "\n";
            }
            else
            {
                std::string fileInfo = line.substr(line.find(" ") + 1);
                newFileIdx += newFileName + " " + fileInfo + "\n";
            }
        }

        // Update the file index
        if (!kvsClient.CPut(userR, "fileIndex.txt", fileIdx, newFileIdx))
        {
            response.status(500, "Internal Server Error");
            response.body("500 Internal Server Error: File index could not be updated");
            response.flush();
            return;
        }

        // Set the response status
        response.status(200, "OK");
        response.flush();
    } catch (std::exception &e) {
        response.status(500, "Internal Server Error");
        response.body("500 Internal Server Error: Exception- " + std::string(e.what()));
        response.flush();
    }
}

void handleGetFolders(const Request &request, Response &response)
{
    try {
        // If the user is not logged in, set an error response
        SessionData sessionData;
        if (!loggedIn(request, sessionData)) 
        {
            response.status(500, "Internal Server Error");
            response.body("Unauthorized");
            response.flush();
            return;
        }

        // Get the current folder path
        std::string folderPath = request.queryParams("path") == "/" ? "" : request.queryParams("path");

        // Check if the folder path is "..", if true, remove the last folder from the path
        std::string folderName = folderPath.substr(folderPath.find_last_of("/") + 1);
        if (folderName == "..")
        {
            // Find the second last "/" in path
            std::size_t found = folderPath.find_last_of("/", folderPath.find_last_of("/") - 1);
            folderPath = folderPath.substr(0, found);
        }

        // check if the user's row exists, if false, create a new row with the user's username
        std::string userR = "./webstorage/" + sessionData.username + "/home" + folderPath;
        std::string folderIdx;
        if (!kvsClient.Get(userR, "folderIndex.txt", folderIdx))
        {
            folderIdx = request.queryParams("path") == "/" ? request.queryParams("path") + "\n" : request.body() + "\n..\n";
            kvsClient.Put(userR, "folderIndex.txt", folderIdx);
        }

        // Iterate through the folderIndex.txt and get the folder info
        std::istringstream folderIndexStream(folderIdx);
        std::string line, jsonResponse = "[";
        std::getline(folderIndexStream, line); // skip the first line which is the current folder's path
        while (std::getline(folderIndexStream, line))
        {
            // convert to json
            jsonResponse += "{\"name\": \"" + line + "\"},";    
        }
        jsonResponse.pop_back(); // remove the last comma
        jsonResponse += "]";

        if (jsonResponse != "]")
        {
            response.status(200, "OK");
            response.type("application/json");
            response.body(jsonResponse);
        }
        else
        {
            response.status(204, "OK");
        }

        // Flush the response
        response.flush();
    } catch (std::exception &e) {
        response.status(500, "Internal Server Error");
        response.body("500 Internal Server Error: Exception- " + std::string(e.what()));
        response.flush();
    }
}

void handleCreateFolder(const Request &request, Response &response)
{
    try {
        // If the user is not logged in, set an error response
        SessionData sessionData;
        if (!loggedIn(request, sessionData)) 
        {
            response.status(500, "Internal Server Error");
            response.body("Unauthorized");
            response.flush();
            return;
        }
        
        // Get the folder name from the request headers
        std::string folderPath = request.body();
        std::string folderParent = folderPath.substr(0, folderPath.find_last_of("/"));
        std::string folderName = folderPath.substr(folderPath.find_last_of("/") + 1);

        // Get the current folders in the user's row
        std::string userR = "./webstorage/" + sessionData.username + "/home" + folderParent;
        std::string folderIdx;
        if (!kvsClient.Get(userR, "folderIndex.txt", folderIdx))
        {
            folderIdx = folderParent.empty() ? "/\n" : folderParent + "\n";
            kvsClient.Put(userR, "folderIndex.txt", folderIdx);
        }

        // Check if folder name already existed
        if (hasFolder(folderIdx, folderName))
        {
            response.status(500, "Internal Server Error");
            response.body("500 Internal Server Error: Folder already exists");
            response.flush();
            return;
        }

        // Create a new row for the new folder in the KVS
        std::string newFolderR = userR + "/" + folderName;
        if (!kvsClient.Put(newFolderR, "folderIndex.txt", folderPath + "\n..\n"))
        {
            response.status(500, "Internal Server Error");
            response.body("500 Internal Server Error: Folder could not be created");
            response.flush();
            return;
        }

        // Save the folder name in KVS and append the new folder info to the folder index 
        std::string newFolderIdx = folderIdx + folderName + "\n";
        if (kvsClient.CPut(userR, "folderIndex.txt", folderIdx, newFolderIdx))
        {
            // create a json response containing the new folder info
            std::string jsonResponse = "{\"name\": \"" + folderName + "\"}";

            // Set the response status
            response.status(200, "OK");
            response.type("application/json");
            response.body(jsonResponse);
        }
        else
        {
            // If the file cannot be opened, set an error response
            response.status(500, "Internal Server Error");
            response.body("500 Internal Server Error: Folder could not be saved");
        }

        // Flush the response
        response.flush();
    } catch (std::exception &e) {
        response.status(500, "Internal Server Error");
        response.body("500 Internal Server Error: Exception- " + std::string(e.what()));
        response.flush();
    }
}

void handleDeleteFolder(const Request &request, Response &response)
{
    try {
        // If the user is not logged in, set an error response
        SessionData sessionData;
        if (!loggedIn(request, sessionData)) 
        {
            response.status(500, "Internal Server Error");
            response.body("Unauthorized");
            response.flush();
            return;
        }

        std::string folderParent = request.queryParams("path") == "/" ? "" : request.queryParams("path");
        std::string folderName = request.params("name");

        // Check if the folder to delete is the current folder
        if (folderParent == folderName)
        {
            response.status(500, "Internal Server Error");
            response.body("500 Internal Server Error: Cannot delete current folder");
            response.flush();
            return;
        }

        // Get the folder index of the current folder
        std::string userR = "./webstorage/" + sessionData.username + "/home" + folderParent; // the row key of the current folder
        std::string folderIdx;
        if (!kvsClient.Get(userR, "folderIndex.txt", folderIdx))
        {
            response.status(404, "Not Found");
            response.body("404 Not Found: Folder index not found");
            response.flush();
            return;
        }

        // Check if the folder to delete exists
        if (!hasFolder(folderIdx, folderName))
        {
            response.status(404, "Not Found");
            response.body("404 Not Found: Folder not found");
            response.flush();
            return;
        }

        // Delete the folder in kvs
        std::string  deleteFolderR = userR + "/" + folderName; // the row key of the folder to delete
        deleteFolder(deleteFolderR);

        // Open the folder index and create a temp file for writing the new index
        std::istringstream folderIdxStream(folderIdx);
        std::string newFolderIdx, line;
        while (std::getline(folderIdxStream, line))
        {
            if (line != folderName)
            {
                newFolderIdx += line + "\n";
            }
        }

        // Update the folder index
        if (!kvsClient.CPut(userR, "folderIndex.txt", folderIdx, newFolderIdx))
        {
            response.status(500, "Internal Server Error");
            response.body("500 Internal Server Error: Folder index could not be updated");
            response.flush();
            return;
        }

        // Set the response status
        response.status(200, "OK");
        response.flush();
    } catch (std::exception &e) {
        response.status(500, "Internal Server Error");
        response.body("500 Internal Server Error: Exception- " + std::string(e.what()));
        response.flush();
    }
}

void handleMoveFolder(const Request &request, Response &response)
{
    try {
        // If the user is not logged in, set an error response
        SessionData sessionData;
        if (!loggedIn(request, sessionData)) 
        {
            response.status(500, "Internal Server Error");
            response.body("Unauthorized");
            response.flush();
            return;
        }

        // Get the folder to move and the destination folder
        std::string folderParent = request.queryParams("path") == "/" ? "" : request.queryParams("path");
        std::string folderName = request.params("name");
        std::string destPath = request.queryParams("dest") == "/" ? "" : request.queryParams("dest");

        // Get the folder name and its new row key
        std::string folderPath = folderParent + "/" + folderName;
        std::string oldFolderR = "./webstorage/" + sessionData.username + "/home" + folderPath;

        std::string newFolderPath = destPath + "/" + folderName;
        std::string newFolderR = "./webstorage/" + sessionData.username + "/home" + newFolderPath;

        // Check if the destination folder is the current folder
        if (folderPath == destPath)
        {
            response.status(500, "Internal Server Error");
            response.body("500 Internal Server Error: Cannot move folder to current folder");
            response.flush();
            return;
        }

        // Open the folder index of the destination folder and create a temp file for writing the new index
        std::string newUserR = "./webstorage/" + sessionData.username + "/home" + destPath;
        std::string newFolderIdx;
        if (!kvsClient.Get(newUserR, "folderIndex.txt", newFolderIdx))
        {
            response.status(404, "Not Found");
            response.body("404 Not Found: Destination Folder index not found");
            response.flush();
            return;
        }

        // Open the folder index of the current folder and create a temp file for writing the new index
        std::string oldUserR = "./webstorage/" + sessionData.username + "/home" + folderParent;
        std::string oldFolderIdx;
        if (!kvsClient.Get(oldUserR, "folderIndex.txt", oldFolderIdx))
        {
            response.status(404, "Not Found");
            response.body("404 Not Found: Folder index not found");
            response.flush();
            return;
        }

        // Check if the folder to move exists
        if (!hasFolder(oldFolderIdx, folderName))
        {
            response.status(404, "Not Found");
            response.body("404 Not Found: Folder not found");
            response.flush();
            return;
        }

        // Move the folder to the destination folder
        moveFolder(oldFolderR, newFolderR, newFolderPath);
        
        // Open the folder index of the old folder parent and create a temp file for writing the new index
        std::istringstream oldFolderIdxStream(oldFolderIdx);
        std::string folderIdx, line;
        while (std::getline(oldFolderIdxStream, line))
        {
            if (line != folderName)
            {
                folderIdx += line + "\n";
            }
        }

        // Update the folder index of the old folder parent
        if (!kvsClient.CPut(oldUserR, "folderIndex.txt", oldFolderIdx, folderIdx))
        {
            response.status(500, "Internal Server Error");
            response.body("500 Internal Server Error: Folder index could not be updated");
            response.flush();
            return;
        }
        
        // Update the folder index of the new folder parent
        std::string newFolderIdxUpdate = newFolderIdx + folderName + "\n";
        if (!kvsClient.CPut(newUserR, "folderIndex.txt", newFolderIdx, newFolderIdxUpdate))
        {
            response.status(500, "Internal Server Error");
            response.body("500 Internal Server Error: Folder index could not be updated");
            response.flush();
            return;
        }

        // Set the response status
        response.status(200, "OK");
        response.flush();
    } catch (std::exception &e) {
        response.status(500, "Internal Server Error");
        response.body("500 Internal Server Error: Exception- " + std::string(e.what()));
        response.flush();
    }
}

void handleRenameFolder(const Request &request, Response &response)
{
    try {
        // If the user is not logged in, set an error response
        SessionData sessionData;
        if (!loggedIn(request, sessionData)) 
        {
            response.status(500, "Internal Server Error");
            response.body("Unauthorized");
            response.flush();
            return;
        }

        // Get the folder to rename and the new folder name
        std::string folderParent = request.queryParams("path") == "/" ? "" : request.queryParams("path");
        std::string folderName = request.params("name");
        std::string newFolderName = request.params("newName");

        if (folderName == newFolderName)
        {
            response.status(200, "OK");
            response.flush();
            return;
        }

        // Get the folder name and its new row key
        std::string folderPath = folderParent + "/" + folderName;
        std::string oldFolderR = "./webstorage/" + sessionData.username + "/home" + folderPath;

        std::string newFolderPath = folderParent + "/" + newFolderName;
        std::string newFolderR = "./webstorage/" + sessionData.username + "/home" + newFolderPath;

        // Open the folder index of the current folder and create a temp file for writing the new index
        std::string userR = "./webstorage/" + sessionData.username + "/home" + folderParent;
        std::string folderIdx;
        if (!kvsClient.Get(userR, "folderIndex.txt", folderIdx))
        {
            response.status(404, "Not Found");
            response.body("404 Not Found: Folder index not found");
            response.flush();
            return;
        }

        // Check if the folder to rename exists
        if (!hasFolder(folderIdx, folderName))
        {
            response.status(404, "Not Found");
            response.body("404 Not Found: Folder not found");
            response.flush();
            return;
        }

        // check if new folder name existed
        if (hasFolder(folderIdx, newFolderName))
        {
            response.status(500, "Internal Server Error");
            response.body("500 Internal Server Error: Folder already exists");
            response.flush();
            return;
        }

        // Move all the subfolders and files to the new renamed folder
        moveFolder(oldFolderR, newFolderR, newFolderPath);

        // Open the folder index of the old folder parent and create a temp file for writing the new index
        std::istringstream folderIdxStream(folderIdx);
        std::string newFolderIdx, line;
        while (std::getline(folderIdxStream, line))
        {
            if (line != folderName)
            {
                newFolderIdx += line + "\n";
            }
            else
            {
                newFolderIdx += newFolderName + "\n";
            }
        }

        // Update the folder index
        if (!kvsClient.CPut(userR, "folderIndex.txt", folderIdx, newFolderIdx))
        {
            response.status(500, "Internal Server Error");
            response.body("500 Internal Server Error: Folder index could not be updated");
            response.flush();
            return;
        }

        // Set the response status
        response.status(200, "OK");
        response.flush();
    } catch (std::exception &e) {
        response.status(500, "Internal Server Error");
        response.body("500 Internal Server Error: Exception- " + std::string(e.what()));
        response.flush();
    }
}

/**
 * We should probably hash the row and column names to avoid any issues with special characters.
*/
