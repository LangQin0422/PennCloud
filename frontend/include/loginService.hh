#pragma once

#include <iostream>
#include <unordered_map>
#include <string>
#include <chrono>  // For std::chrono::system_clock
#include <sstream> // For std::stringstream
#include <random>
#include <shared_mutex>
#include "KVSClient.hpp"

using std::string;
using std::unordered_map;

// Use a struct to hold session data and expiration time
struct SessionData
{
    string username;
    string password;
    std::chrono::system_clock::time_point expiry;
};

unordered_map<string, SessionData> activeSessions;
std::shared_mutex sessionsMutex;

/**
 * Handles requests for the index page.
 *
 * @param request The request object.
 * @param response The response object.
 */
void handleIndexPage(const Request &request, Response &response)
{
    // Open the file
    std::ifstream file("./public/index.html");
    if (file.is_open())
    {
        // Read the file content into a string stream
        std::ostringstream buffer;
        buffer << file.rdbuf();
        std::string fileContent = buffer.str();

        // Set the response body to the content of the file
        response.body(fileContent);

        // Set the response headers
        response.type("text/html");
        response.status(200, "OK");
    }
    else
    {
        // If the file cannot be opened, set an error response
        response.status(404, "Not Found");
        response.body("404 Not Found: File not found");
    }

    // Flush the response
    response.flush();
}

/**
 * Handles requests for the logo image.
 *
 * @param request The request object.
 * @param response The response object.
 */
void handleLogoImage(const Request &request, Response &response)
{
    // Open the image file
    std::ifstream file("./public/images/logo.png", std::ios::binary);

    if (file.is_open())
    {
        // Read the image file content into a string stream
        std::ostringstream buffer;
        buffer << file.rdbuf();
        std::string imageContent = buffer.str();

        // Set the response body to the content of the image
        response.body(imageContent);

        // Set the response headers for an image
        response.type("image/png");
        response.status(200, "OK");
    }
    else
    {
        // If the file cannot be opened, set an error response
        response.status(404, "Not Found");
        response.body("404 Not Found: Image file not found");
    }

    // Flush the response
    response.flush();
}

/**
 * Removes expired sessions in a separate thread.
 */
void removeExpiredSessions()
{
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::minutes(5));

        auto now = std::chrono::system_clock::now();

        std::unique_lock<std::shared_mutex> guard(sessionsMutex); // Exclusive lock for writing

        for (auto it = activeSessions.begin(); it != activeSessions.end();)
        {
            if (now >= it->second.expiry)
            {
                it = activeSessions.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
}

/**
 * Generates a session token for the given username.
 *
 * @param username The username for which to generate the session token.
 * @return The generated session token.
 */
std::string generateSessionToken(const std::string &username)
{
    std::random_device rd;                                       // Non-deterministic random number generator
    std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX); // Distribute over the entire uint64_t range

    std::stringstream ss;
    ss << username << "-" << dist(rd); // Concatenate username and random number
    return ss.str();
}

/**
 * Checks if a user is logged in and retrieves session data if logged in.
 *
 * @param req The request object.
 * @param output A reference to the session data output.
 * @return True if the user is logged in, false otherwise.
 */
bool loggedIn(const Request &req, SessionData &output)
{
    string cookieHeader = req.headers("cookie");
    std::string sessionTokenKey = "SessionToken=";
    size_t start = cookieHeader.find(sessionTokenKey);

    if (start == std::string::npos)
    {
        return false;
    }

    start += sessionTokenKey.length();
    size_t end = cookieHeader.find(';', start);
    std::string sessionToken = end == std::string::npos ? cookieHeader.substr(start) : cookieHeader.substr(start, end - start);

    std::shared_lock<std::shared_mutex> guard(sessionsMutex); // Shared lock for reading

    auto it = activeSessions.find(sessionToken);
    if (it == activeSessions.end())
    {
        return false;
    }

    output = it->second;
    return true;
}

/**
 * Assigns a session token to the given username and password, and sets it as a cookie in the response.
 *
 * @param username The username for which to assign the session token.
 * @param password The password associated with the username.
 * @param rsp The response object to set the session token cookie.
 */
void assignSessionToken(const string &username, const string &password, Response &rsp)
{
    // Generate a session token
    string sessionToken = generateSessionToken(username);

    // Store it in the activeSessions map
    auto expiry = std::chrono::system_clock::now() + std::chrono::hours(1);
    std::unique_lock<std::shared_mutex> guard(sessionsMutex); // Exclusive lock for writing
    activeSessions[sessionToken] = {username, password, expiry};
    const string assignedURL = "";

    // Set the session token as a cookie in the response
    string cookie = "SessionToken=" + sessionToken + "; Path=/; HttpOnly; Max-Age=3600";

    rsp.header("Set-Cookie", cookie);
}

/**
 * Handles login requests by verifying credentials and assigning a session token upon successful login.
 *
 * @param req The request object containing the username and password headers.
 * @param rsp The response object to send the login response.
 */
void handleLogin(const Request &req, Response &rsp)
{
    const string &username = req.headers("username");
    const string &password = req.headers("password");
    string expectedPassword;
    std::cout << "Try checking the password in the KVS" << std::endl;
    if (!username.empty() && !password.empty() && kvsClient.Get("accounts", username, expectedPassword, "LOCK_BYPASS") && expectedPassword == password)
    {
        assignSessionToken(username, password, rsp);
        rsp.body("success");
        rsp.type("text/plain");
        rsp.header("Access-Control-Allow-Origin", "*");                   // Allow any domain
        rsp.header("Access-Control-Allow-Methods", "GET, POST, OPTIONS"); // Allowed methods
        rsp.header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        rsp.status(200, "OK");
        // print rsp.formatHTML()
        std::cout << rsp.formatHTML() << std::endl;
        std::cout << "------------------------" << std::endl;
        rsp.flush();
    }
    else
    {
        rsp.body("invalid password or username");
        rsp.header("Access-Control-Allow-Origin", "*");                   // Allow any domain
        rsp.header("Access-Control-Allow-Methods", "GET, POST, OPTIONS"); // Allowed methods
        rsp.header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        rsp.type("text/plain");
        rsp.status(200, "OK");
        std::cout << rsp.formatHTML() << std::endl;
        std::cout << "------------------------" << std::endl;
        rsp.flush();
    }
}

/**
 * Handles user sign up requests by creating a new account if the username is available.
 *
 * @param req The request object containing the username and password headers.
 * @param rsp The response object to send the sign-up response.
 */
void handleSignUp(const Request &req, Response &rsp)
{
    const string &username = req.headers("username");
    const string &password = req.headers("password");
    string value;
    if (!username.empty() && !password.empty() && !kvsClient.Get("accounts", username, value, "LOCK_BYPASS"))
    {
        // sign up success
        assignSessionToken(username, password, rsp);
        kvsClient.Put("accounts", username, password, "LOCK_BYPASS");
        rsp.body("success");
        rsp.type("text/plain");
        rsp.header("Access-Control-Allow-Origin", "*");                   // Allow any domain
        rsp.header("Access-Control-Allow-Methods", "GET, POST, OPTIONS"); // Allowed methods
        rsp.header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        rsp.status(200, "OK");
        rsp.flush();
    }
    else
    {
        rsp.body("username already exists");
        rsp.type("text/plain");
        rsp.header("Access-Control-Allow-Origin", "*");                   // Allow any domain
        rsp.header("Access-Control-Allow-Methods", "GET, POST, OPTIONS"); // Allowed methods
        rsp.header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        rsp.status(409, "Conflict");
        rsp.flush();
    }
}

/**
 * Checks if a user is logged in and returns their username if logged in.
 *
 * @param req The request object.
 * @param rsp The response object to send the logged-in status or username.
 */
void handleIsLoggedIn(const Request &req, Response &rsp)
{
    SessionData sessiondata;
    if (loggedIn(req, sessiondata))
    {
        // if already loggedin, return username
        rsp.body(sessiondata.username);
        rsp.header("Access-Control-Allow-Origin", "*");                   // Allow any domain
        rsp.header("Access-Control-Allow-Methods", "GET, POST, OPTIONS"); // Allowed methods
        rsp.header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        rsp.type("text/plain");
        rsp.status(200, "OK");
        rsp.flush();
    }
    else
    {
        rsp.body("false");
        rsp.header("Access-Control-Allow-Origin", "*");                   // Allow any domain
        rsp.header("Access-Control-Allow-Methods", "GET, POST, OPTIONS"); // Allowed methods
        rsp.header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        rsp.type("text/plain");
        rsp.status(401, "Unauthorized");
        rsp.flush();
    }
}

/**
 * Handles user logout requests by invalidating the session token.
 *
 * @param req The request object.
 * @param rsp The response object to send the logout status.
 */
void handleLogout(const Request &req, Response &rsp)
{
    string cookieHeader = req.headers("cookie");
    std::string sessionTokenKey = "SessionToken=";
    size_t start = cookieHeader.find(sessionTokenKey);

    if (start == std::string::npos)
    {
        rsp.body("false");
        rsp.type("text/plain");
        rsp.status(401, "Unauthorized");
        rsp.flush();
        return;
    }

    start += sessionTokenKey.length();
    size_t end = cookieHeader.find(';', start);
    std::string sessionToken = end == std::string::npos ? cookieHeader.substr(start) : cookieHeader.substr(start, end - start);

    std::unique_lock<std::shared_mutex> guard(sessionsMutex); // Exclusive lock for writing
    auto it = activeSessions.find(sessionToken);
    if (it == activeSessions.end())
    {
        rsp.body("false");
        rsp.type("text/plain");
        rsp.status(401, "Unauthorized");
        rsp.flush();
        return;
    }
    // Remove the session from activeSessions
    activeSessions.erase(sessionToken);
    rsp.body("true");
    rsp.type("text/plain");
    rsp.status(200, "OK");
    rsp.flush();
}

/**
 * Handles requests to change a user's password.
 *
 * @param req The request object containing the username, old password, and new password headers.
 * @param rsp The response object to send the password change response.
 */
void handleChangePassword(const Request &req, Response &rsp)
{
    SessionData sessiondata;
    if (loggedIn(req, sessiondata))
    {
        const std::string oldPassword = req.headers("oldPassword");
        const std::string newPassword = req.headers("newPassword");
        if (oldPassword != sessiondata.password)
        {
            rsp.body("Old password is incorrect");
            rsp.type("text/plain");
            rsp.header("Access-Control-Allow-Origin", "*");                   // Allow any domain
            rsp.header("Access-Control-Allow-Methods", "GET, POST, OPTIONS"); // Allowed methods
            rsp.header("Access-Control-Allow-Headers", "Content-Type, Authorization");
            rsp.status(401, "Unauthorized");
            rsp.flush();
            return;
        }
        if (kvsClient.CPut("accounts", sessiondata.username, oldPassword, newPassword, "LOCK_BYPASS"))
        {
            rsp.body("success");
            rsp.type("text/plain");
            rsp.header("Access-Control-Allow-Origin", "*");                   // Allow any domain
            rsp.header("Access-Control-Allow-Methods", "GET, POST, OPTIONS"); // Allowed methods
            rsp.header("Access-Control-Allow-Headers", "Content-Type, Authorization");
            rsp.status(200, "OK");
            rsp.flush();
            sessiondata.password = newPassword;
            return;
        }
        else
        {
            rsp.body("Failed to change password, please try again");
            rsp.type("text/plain");
            rsp.header("Access-Control-Allow-Origin", "*");                   // Allow any domain
            rsp.header("Access-Control-Allow-Methods", "GET, POST, OPTIONS"); // Allowed methods
            rsp.header("Access-Control-Allow-Headers", "Content-Type, Authorization");
            rsp.status(500, "Internal Server Error");
            rsp.flush();
            return;
        }
    }
    else
    {
        rsp.body("You were logged out.");
        rsp.header("Access-Control-Allow-Origin", "*");                   // Allow any domain
        rsp.header("Access-Control-Allow-Methods", "GET, POST, OPTIONS"); // Allowed methods
        rsp.header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        rsp.type("text/plain");
        rsp.status(401, "Unauthorized");
        rsp.flush();
    }
    return;
}
