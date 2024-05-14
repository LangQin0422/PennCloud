#pragma once
#ifndef HANDLER_HH
#define HANDLER_HH

#include "server.hh"

/**
 * Handles requests for the index page.
 *
 * @param request The request object.
 * @param response The response object.
 */
void handleIndexPage(const Request &request, Response &response);

/**
 * Handles ping requests from workers.
 *
 * @param request The request object.
 * @param response The response object.
 */
void handlePing(const Request &request, Response &response);

/**
 * Handles registration requests from clients.
 *
 * @param request The request object.
 * @param response The response object.
 */
void handleRegister(const Request &request, Response &response);

/**
 * Handles requests for the admin page.
 *
 * @param request The request object.
 * @param response The response object.
 */
void handleAdmin(const Request &request, Response &response);

/**
 * Handles requests for the table page.
 *
 * @param request The request object.
 * @param response The response object.
 */
void handleTable(const Request &request, Response &response);

/**
 * Handles requests for the entry page.
 *
 * @param request The request object.
 * @param response The response object.
 */
void handleEntry(const Request &request, Response &response);

/**
 * Handles API requests to retrieve information about active workers.
 *
 * @param request The request object.
 * @param response The response object.
 */
void handleAPIWorkers(const Request &request, Response &response);

/**
 * Handles API requests to retrieve information about Key-Value Stores (KVS).
 *
 * @param request The request object.
 * @param response The response object.
 */
void handleAPIKVS(const Request &request, Response &response);

/**
 * Handles API requests to kill a specific Key-Value Store (KVS) server.
 *
 * @param request The request object.
 * @param response The response object.
 */
void handleAPIKillKVS(const Request &request, Response &response);

/**
 * Handles API requests to start a Key-Value Store (KVS) server.
 *
 * @param request The request object.
 * @param response The response object.
 */
void handleAPIStartKVS(const Request &request, Response &response);

/**
 * Escapes special characters in a JSON string.
 *
 * @param s The string to escape.
 * @return The escaped string.
 */
std::string escapeJSON(const std::string &s);

/**
 * Handles API requests to retrieve all rows from the Key-Value Store (KVS).
 *
 * @param request The request object.
 * @param response The response object.
 */
void handleAPIAllRows(const Request &request, Response &response);

/**
 * Splits a string into tokens using a delimiter.
 *
 * @param input The input string to split.
 * @param delimiter The delimiter character.
 * @return A vector containing the tokens.
 */
std::vector<std::string> splitString(const std::string &input, char delimiter);

/**
 * Initializes the Key-Value Store (KVS).
 */
void initKVS();

#endif
