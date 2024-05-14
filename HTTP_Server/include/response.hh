#pragma once

#ifndef RESPONSE_HH
#define RESPONSE_HH

#include <string>

class Response
{
public:
  virtual ~Response() = default;

  virtual void body(const std::string &body) = 0;
  virtual void body(const char *body) = 0;
  virtual void header(const std::string &name, const std::string &value) = 0;
  virtual void type(const std::string &contentType) = 0;
  virtual void status(int statusCode, const std::string &reasonPhrase) = 0;

  /**
   * Formats the response as an HTML string.
   *
   * This method formats the response as an HTML string suitable for sending over the network.
   * It constructs the response line including the HTTP status code and reason phrase, adds
   * any specified headers, ensures that the Content-Length header is set if there is a body,
   * and appends the response body. The formatted HTML string is returned.
   *
   * @return A string containing the formatted HTML response.
   */
  virtual std::string formatHTML() = 0;

  /**
   * Sends the formatted HTML response over the network.
   *
   * This method retrieves the formatted HTML response using the formatHTML() method, and then
   * writes the response to the specified socket. It converts the response string to a C-style
   * string and writes it to the socket. The method does not return any value.
   *
   */
  virtual void flush() = 0;
};

#endif // RESPONSE_HH