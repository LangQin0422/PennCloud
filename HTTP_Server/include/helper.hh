#pragma once
#ifndef HELPER_HH
#define HELPER_HH

#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <netinet/in.h>
#include <iostream>

#include "server.hh"

/**
 * Parses the query parameters from a URL string and returns them as a key-value map.
 *
 * This function extracts the query string part of a URL, following the '?' character,
 * and splits it into key-value pairs based on '&' delimiters. Each pair is then
 * split into a key and value based on the '=' delimiter. The function populates
 * and returns an unordered_map where each key is a unique query parameter name
 * and the corresponding value is the parameter's value.
 *
 * @param url The URL string from which query parameters are to be extracted.
 * @return A std::unordered_map<std::string, std::string> containing the query
 *         parameters as key-value pairs. If the URL does not contain any query
 *         parameters, an empty map is returned.
 *
 * Example:
 *     URL: "http://example.com/page?param1=value1&param2=value2"
 *     Returns: {{"param1", "value1"}, {"param2", "value2"}}
 */
std::unordered_map<std::string, std::string> parseQueryParams(const std::string &url);

/**
 * Extracts the path from a full URL, removing any query parameters.
 *
 * This function takes a URL or a path that may include query parameters,
 * identified by the presence of the '?' character. It extracts and returns
 * the portion of the URL or path before the '?' character, effectively removing
 * any query parameters. If the '?' character is not found, indicating that there
 * are no query parameters, the original URL or path is returned unchanged.
 *
 * @param fullPath The full URL or path, possibly including query parameters.
 * @return A std::string containing the path without query parameters.
 *         If no query parameters are present, the original fullPath is returned.
 *
 * Example:
 *     Input: "/example/path?param=value&another=thing"
 *     Returns: "/example/path"
 */
std::string extractPath(const std::string &fullPath);

/**
 * Splits a string into substrings based on a specified delimiter character.
 *
 * This function iterates over the given string, segmenting it into substrings
 * whenever the specified delimiter character is encountered. Each of these
 * substrings is then added to a vector. If the delimiter is not found, the
 * entire string is added as a single element in the vector. Empty substrings
 * can also be added to the vector if the delimiter character is present at the
 * beginning or end of the string, or if there are consecutive delimiters within
 * the string.
 *
 * @param s The string to be split.
 * @param delimiter The character used to split the string.
 * @return A std::vector<std::string> containing all the substrings extracted
 *         from the original string. If the original string does not contain
 *         the delimiter, the vector will contain the original string as its
 *         only element.
 *
 * Example:
 *     Input: "one,two,three", delimiter: ','
 *     Returns: {"one", "two", "three"}
 */
std::vector<std::string> split(const std::string &s, char delimiter);

/**
 * Attempts to match a given request path with a set of registered routes, considering dynamic parameters.
 *
 * This function compares a request path against a set of predefined routes, attempting to find a match. Routes
 * may contain dynamic segments, indicated by a colon (:) prefix, which can match any string segment in the
 * request path. If a match is found, the function extracts dynamic parameters from the request path and stores
 * them in a key-value map, where the key is the parameter name (without the colon) and the value is the corresponding
 * segment from the request path.
 *
 * @param method The HTTP method of the request. This implementation does not use it but allows for future extension.
 * @param requestPath The path of the incoming HTTP request to match against the routes.
 * @param matchedPath A reference to a string that will store the path of the matched route, if any.
 * @param routes A reference to an unordered_map where keys are route paths (with potential dynamic segments)
 *               and values are RouteHandler functions associated with those paths.
 * @param extractedParams A reference to an unordered_map that will store extracted parameters from the dynamic
 *                        segments of the matched route.
 * @return A boolean value indicating whether a matching route was found. True if a match was found, false otherwise.
 *
 * Example:
 *     Input: method = "GET", requestPath = "/user/123/profile", routes contains "/user/:id/profile"
 *     After execution, extractedParams will contain {"id" : "123"} if the route matches.
 */
bool matchRoute(const std::string &method, const std::string &requestPath,
                std::string &matchedPath,
                std::unordered_map<std::string, RouteHandler> &routes,
                std::unordered_map<std::string, std::string> &extractedParams);

/**
 * Checks if a file exists at the specified path.
 *
 * @param path The path to the file to check.
 * @return True if the file exists, false otherwise.
 */
bool fileExists(const std::string &path);

/**
 * Determines the MIME type of a file based on its extension.
 *
 * @param filePath The path to the file.
 * @return The MIME type corresponding to the file extension.
 */
std::string getMimeType(const std::string &filePath);

/**
 * Trims leading and trailing whitespace from a string.
 *
 * @param str The string to trim.
 * @return The trimmed string.
 */
std::string trim(const std::string &str);
/**
 * Retrieves the content length from HTTP headers.
 *
 * @param headers The map containing HTTP headers.
 * @return The content length as an integer. Returns 0 if Content-Length is not present.
 */
int getContentLength(const std::unordered_map<std::string, std::string> &headers);

/**
 * Parses HTTP headers from a string and returns them as a map.
 *
 * @param headerStr The string containing HTTP headers.
 * @return The parsed HTTP headers as a map.
 */
std::unordered_map<std::string, std::string> parseHeaders(const std::string &headerStr);

/**
 * Generates a random ID of specified length.
 *
 * @param length The length of the random ID to generate.
 * @return The generated random ID.
 */
std::string generateRandomID(size_t length);

/**
 * Checks if a string is empty or consists only of whitespace characters.
 *
 * @param str The string to check.
 * @return True if the string is empty or consists only of whitespace, false otherwise.
 */
bool isEmptyOrWhitespace(const std::string &str);

/**
 * Parses a configuration file and populates a Config object.
 *
 * @param filename The name of the configuration file to parse.
 * @param config The Config object to populate with parsed data.
 * @throws std::runtime_error if the file cannot be opened.
 */
void parseConfig(const std::string &filename, Config &config);

#endif // HELPER_HH
