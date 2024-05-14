#pragma once

#ifndef REQUEST_HH
#define REQUEST_HH

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

class Request
{
public:
  virtual std::string ip() const = 0;
  virtual int port() const = 0;

  virtual std::string method() const = 0;

  /**
   * Retrieves the URL from the client's request, excluding any query parameters.
   *
   * This method returns the URL requested by the client, up to but not including the query string.
   * If the original URL contains query parameters (indicated by the presence of a '?'), this method
   * strips them from the URL before returning it. This is useful for routing and handling requests
   * where the path is significant but the specific query parameters are handled separately.
   *
   * @return A std::string containing the URL of the request without query parameters. If there are
   *         no query parameters in the original request URL, the entire URL is returned unchanged.
   */
  virtual std::string url() const = 0;
  virtual std::string protocol() const = 0;

  /**
   * Retrieves a set of all header names included in the client's request.
   *
   * This method iterates over all headers present in the request and collects their names into
   * an unordered set. This allows for easy identification of which headers were provided by the
   * client without considering the specific values of those headers. The names are collected
   * as they are stored in the request, typically in lowercase to standardize HTTP header names.
   *
   * @return An std::unordered_set<std::string> containing the names of all headers included in
   *         the request. If the request does not contain any headers, an empty set is returned.
   */
  virtual std::unordered_set<std::string> headers() const = 0;

  /**
   * Retrieves the value of a specific header from the client's request.
   *
   * This method searches for a header by name in the request's headers. It is case-insensitive,
   * as it converts the provided header name to lowercase before searching. This conforms to the
   * HTTP/1.1 specification, where header names are case-insensitive. If the header is found,
   * its value is returned. If the header is not present in the request, a default "NOT FOUND"
   * string is returned. It's important to note that returning a hardcoded "NOT FOUND" string
   * might not be the best practice for all use cases, and checking for the presence of a header
   * might be better handled by the calling code.
   *
   * @param name The name of the header to retrieve, case-insensitive.
   * @return A std::string containing the value of the header if it exists in the request.
   *         If the header is not found, returns "NOT FOUND".
   */
  virtual std::string headers(const std::string &name) const = 0;

  /**
   * Sets the value of a header in the response.
   *
   * This method allows for setting or updating the value of a specific header in the HTTP response.
   * The header name is converted to lowercase before it is stored, adhering to the convention that
   * HTTP header names are case-insensitive. This ensures consistent handling and retrieval of headers.
   * If the header already exists, its value is updated; otherwise, a new header with the specified
   * name and value is added to the response.
   *
   * @param name The name of the header to set or update. The name is treated case-insensitively.
   * @param value The value to assign to the header.
   */
  virtual void setHeader(const std::string &name, const std::string &value) = 0;
  virtual std::string contentType() const = 0;

  virtual std::string body() const = 0;
  virtual const char *bodyAsBytes() const = 0;
  virtual int contentLength() const = 0;

  /**
   * Retrieves a set of all query parameter names included in the client's request URL.
   *
   * This method collects the names of all query parameters from the request's URL into
   * an unordered set. This is useful for quickly determining which query parameters were
   * provided by the client without accessing their specific values. The names are collected
   * as they appear in the URL.
   *
   * @return An std::unordered_set<std::string> containing the names of all query parameters
   *         included in the request. If the request URL does not contain any query parameters,
   *         an empty set is returned.
   */
  virtual std::unordered_set<std::string> queryParams() const = 0;

  /**
   * Retrieves the value of a specific query parameter included in the client's request URL.
   *
   * This method retrieves the value of a query parameter specified by its name from the
   * request's URL. If the requested parameter exists in the URL, its corresponding value
   * is returned. If the parameter is not found in the URL, the string "NOT FOUND" is returned.
   *
   * @param param The name of the query parameter whose value is to be retrieved.
   * @return A string containing the value of the specified query parameter if found,
   *         or "NOT FOUND" if the parameter is not present in the request URL.
   */
  virtual std::string queryParams(const std::string &param) const = 0;

  virtual std::unordered_map<std::string, std::string> params() const = 0;

  /**
   * Retrieves the value of a specific named parameter included in the request URL path.
   *
   * This method retrieves the value of a named parameter specified by its name from the
   * request's URL path. If the requested parameter exists in the URL path, its corresponding
   * value is returned. If the parameter is not found in the URL path, the string "NOT FOUND"
   * is returned.
   *
   * @param name The name of the named parameter whose value is to be retrieved.
   * @return A string containing the value of the specified named parameter if found,
   *         or "NOT FOUND" if the parameter is not present in the request URL path.
   */
  virtual std::string params(const std::string &name) const = 0;

  /**
   * Sets the parameters for the request.
   *
   * @param params The map containing parameters.
   */
  virtual void setParams(const std::unordered_map<std::string, std::string> &params) = 0;

  /**
   * Prints the details of the request to the standard output.
   */
  virtual void print() const = 0;

  virtual ~Request() {}
};

#endif // REQUEST_HH