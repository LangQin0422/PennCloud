#pragma once

#ifndef RESPONSEIMP_HH
#define RESPONSEIMP_HH

#include "response.hh"
#include <unordered_map>
#include <sstream>

class ResponseImp : public Response
{
private:
  int _socket;
  std::string _method = "GET";
  std::string _body;
  std::unordered_map<std::string, std::string> _headers;
  int _statusCode = 200;                  // Default to 200 OK
  std::string _reasonPhrase = "OK";       // Default reason phrase
  std::string _contentType = "text/html"; // Default Content-Type

public:
  ResponseImp(const int socket, const std::string method) : _socket(socket), _method(method){};
  virtual ~ResponseImp() override = default;

  void body(const std::string &body) override
  {
    _body = body;
  }

  void body(const char *body) override
  {
    _body = std::string(body);
  }

  void header(const std::string &name, const std::string &value) override
  {
    _headers[name] = value;
  }

  void type(const std::string &contentType) override
  {
    _contentType = contentType;
    _headers["Content-Type"] = _contentType; // Ensure the Content-Type header is set
  }

  void status(int statusCode, const std::string &reasonPhrase) override
  {
    _statusCode = statusCode;
    _reasonPhrase = reasonPhrase;
  }

  std::string formatHTML() override
  {
    std::ostringstream responseStream;
    responseStream << "HTTP/1.1 " << _statusCode << " " << _reasonPhrase << "\r\n";
    for (const auto &header : _headers)
    {
      responseStream << header.first << ": " << header.second << "\r\n";
    }
    if (!_body.empty())
    {
      // Ensure Content-Length is set if there's a body
      responseStream << "Content-Length: " << _body.length() << "\r\n";
    }
    responseStream << "\r\n"; // End of headers
    responseStream << _body;
    return responseStream.str();
  }

  void flush() override
  {
    if (_method != "HEAD")
    {
      std::string response = formatHTML();
      write(_socket, response.c_str(), response.length());
    }
    // fix HEAD later
  }
};

#endif // RESPONSEIMP_HH