#pragma once

#ifndef REQUESTIMP_HH
#define REQUESTIMP_HH

#include <memory>
#include <netinet/in.h>
#include <cassert>
#include <string>
#include <unordered_map>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <sstream>
#include <arpa/inet.h>
#include <iostream>

#include "request.hh"
#include "helper.hh"

class RequestImp : public Request
{
private:
    std::string _method;
    std::string _url;
    std::string _protocol;
    std::string _body;
    std::unordered_map<std::string, std::string> _headers;
    std::unordered_map<std::string, std::string> _queryParams;
    std::unordered_map<std::string, std::string> _params;
    sockaddr_in _remoteAddr;

public:
    RequestImp(const std::string &methodArg, const std::string &urlArg,
               const std::string &protocolArg, const std::string &bodyRawArg,
               const std::unordered_map<std::string, std::string> &headersArg,
               const std::unordered_map<std::string, std::string> &queryParamsArg,
               const std::unordered_map<std::string, std::string> &paramsArg,
               const sockaddr_in &remoteAddrArg)
        : _method(methodArg), _url(urlArg), _protocol(protocolArg),
          _body(bodyRawArg), _queryParams(queryParamsArg),
          _params(paramsArg), _remoteAddr(remoteAddrArg)
    {
        for (const auto &header : headersArg)
        {
            std::string lowerName = trim(header.first);
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
            _headers[lowerName] = trim(header.second);
        }
    }

    virtual ~RequestImp() override = default;

    std::string ip() const override
    {
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(_remoteAddr.sin_addr), ip, INET_ADDRSTRLEN);
        return std::string(ip);
    }

    int port() const override
    {
        return ntohs(_remoteAddr.sin_port);
    }

    std::string method() const override
    {
        return _method;
    }

    std::string url() const override
    {
        // Assuming url contains the full URL
        std::string urlCopy = _url;
        size_t pos = urlCopy.find("?");
        if (pos != std::string::npos)
        {
            urlCopy = urlCopy.substr(0, pos);
        }
        return urlCopy;
    }

    std::string protocol() const override
    {
        return _protocol;
    }

    std::unordered_set<std::string> headers() const override
    {
        std::unordered_set<std::string> headerNames;
        for (const auto &header : _headers)
        {
            headerNames.insert(header.first);
        }
        return headerNames;
    }

    std::string headers(const std::string &name) const override
    {
        std::string lowerName = name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

        auto it = _headers.find(lowerName);
        if (it != _headers.end())
        {
            return it->second;
        }
        return "not found";
    }

    void setHeader(const std::string &name, const std::string &value) override
    {
        std::string lowerName = name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        _headers[lowerName] = value;
    }

    std::string contentType() const override
    {
        return headers("content-type");
    }

    std::string body() const override
    {
        return _body;
    }

    const char *bodyAsBytes() const override
    {
        return _body.c_str();
    }

    int contentLength() const override
    {
        return _body.size();
    }

    std::unordered_set<std::string> queryParams() const override
    {
        std::unordered_set<std::string> queryParamNames;
        for (const auto &queryParam : _queryParams)
        {
            queryParamNames.insert(queryParam.first);
        }
        return queryParamNames;
    }

    std::string queryParams(const std::string &param) const override
    {
        auto it = _queryParams.find(param);
        if (it != _queryParams.end())
        {
            return it->second;
        }
        return "";
    }

    std::unordered_map<std::string, std::string> params() const override
    {
        return _params;
    }

    std::string params(const std::string &name) const override
    {
        auto it = _params.find(name);
        if (it != _params.end())
        {
            return it->second;
        }
        return "";
    }

    void setParams(const std::unordered_map<std::string, std::string> &params) override
    {
        _params = params;
    }

    void print() const override
    {
        std::cout << "Method: " << method() << std::endl;
        std::cout << "URL: " << url() << std::endl;
        std::cout << "Protocol: " << protocol() << std::endl;
        std::cout << "IP: " << ip() << std::endl;
        std::cout << "Port: " << port() << std::endl;
        std::cout << "Headers: " << std::endl;
        for (const auto &header : headers())
        {
            std::cout << "    " << header << ": " << headers(header) << std::endl;
        }
        std::cout << "Query Params: " << std::endl;
        for (const auto &queryParam : queryParams())
        {
            std::cout << "    " << queryParam << ": " << queryParams(queryParam) << std::endl;
        }
        std::cout << "Params: " << std::endl;
        for (const auto &param : params())
        {
            std::cout << "    " << param.first << ": " << param.second << std::endl;
        }
        std::cout << "Body:\n"
                  << body() << std::endl;
    }
};

#endif // REQUESTIMP_HH