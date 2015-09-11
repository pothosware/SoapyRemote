// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyRPCSocket.hpp"
#include <cstring> //memset
#include <iostream>
#include <udt.h>
#ifndef WIN32
#include <netdb.h>
#endif

static bool lookupURL(const std::string &url,
    int &af, int &type, int &prot,
    struct sockaddr &addr, int &addrlen,
    std::string errorMsg)
{
    //parse the url into the node and service
    std::string node, service;
    bool inBracket = false;
    bool inService = false;
    for (size_t i = 0; i < url.size(); i++)
    {
        const char ch = url[i];
        if (inBracket and ch == ']')
        {
            inBracket = false;
            continue;
        }
        if (not inBracket and ch == '[')
        {
            inBracket = true;
            continue;
        }
        if (inBracket)
        {
            node += ch;
            continue;
        }
        if (inService)
        {
            service += ch;
            continue;
        }
        if (not inService and ch == ':')
        {
            inService = true;
            continue;
        }
        if (not inService)
        {
            node += ch;
            continue;
        }
    }

    if (node.empty())
    {
        errorMsg = "url parse failed";
        return false;
    }

    if (service.empty() or service == "0")
    {
        service = SOAPY_REMOTE_DEFAULT_SERVICE;
    }

    type = SOCK_STREAM;

    //configure the hint
    struct addrinfo hints, *servinfo = NULL;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
    hints.ai_socktype = SOCK_STREAM;

    //get address info
    int ret = getaddrinfo(node.c_str(), NULL/*service.c_str()*/, &hints, &servinfo);
    if (ret != 0)
    {
        errorMsg = gai_strerror(ret);
        return false;
    }

    //iterate through possible matches
    struct addrinfo *p = NULL;
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        //eliminate unsupported family types
        if (p->ai_family != AF_INET and p->ai_family != AF_INET6) continue;

        //found a match
        af = p->ai_family;
        prot = p->ai_protocol;
        addrlen = p->ai_addrlen;
        std::memcpy(&addr, p->ai_addr, p->ai_addrlen);
        break;
    }

    //cleanup
    freeaddrinfo(servinfo);

    if (p == NULL) errorMsg = "no lookup results";
    return p != NULL;
}

SoapyRPCSocket::SoapyRPCSocket(void):
    _sock(UDT::INVALID_SOCK)
{
    return;
}

SoapyRPCSocket::~SoapyRPCSocket(void)
{
    if (this->close() != 0)
    {
        std::cerr << "SoapyRPCSocket::~SoapyRPCSocket: " << UDT::getlasterror().getErrorMessage() << std::endl;
    }
}

bool SoapyRPCSocket::null(void)
{
    return _sock == UDT::INVALID_SOCK;
}

int SoapyRPCSocket::close(void)
{
    if (this->null()) return 0;
    int ret = UDT::close(_sock);
    _sock = UDT::INVALID_SOCK;
    return ret;
}

int SoapyRPCSocket::bind(const std::string &url)
{
    int af = 0, type = 0, prot = 0;
    struct sockaddr addr;
    int addrlen = sizeof(addr);
    std::string errorMsg;

    if (not lookupURL(url, af, type, prot, addr, addrlen, errorMsg))
    {
        std::cerr << "SoapyRPCSocket::bind(" << url << ") Lookup failed: " << errorMsg << std::endl;
        return UDT::ERROR;
    }

    _sock = UDT::socket(af, type, prot);
    if (this->null()) return UDT::ERROR;
    return UDT::bind(_sock, &addr, addrlen);
}

int SoapyRPCSocket::listen(int backlog)
{
    return UDT::listen(_sock, backlog);
}

SoapyRPCSocket SoapyRPCSocket::accept(void)
{
    struct sockaddr addr;
    int addrlen = sizeof(addr);
    SoapyRPCSocket clientSock;
    clientSock._sock = UDT::accept(_sock, &addr, &addrlen);
    return clientSock;
}

int SoapyRPCSocket::connect(const std::string &url)
{
    int af = 0, type = 0, prot = 0;
    struct sockaddr addr;
    int addrlen = sizeof(addr);
    std::string errorMsg;

    if (not lookupURL(url, af, type, prot, addr, addrlen, errorMsg))
    {
        std::cerr << "SoapyRPCSocket::connect(" << url << ") Lookup failed: " << errorMsg << std::endl;
        return UDT::ERROR;
    }

    _sock = UDT::socket(af, type, prot);
    if (this->null()) return UDT::ERROR;
    return UDT::connect(_sock, &addr, addrlen);
}

int SoapyRPCSocket::send(const void *buf, size_t len, int flags)
{
    return UDT::send(_sock, (const char *)buf, int(len), flags);
}

int SoapyRPCSocket::recv(void *buf, size_t len, int flags)
{
    return UDT::recv(_sock, (char *)buf, int(len), flags);
}

bool SoapyRPCSocket::selectRecv(const long timeoutUs)
{
    struct timeval tv;
    tv.tv_sec = timeoutUs / 1000000;
    tv.tv_usec = timeoutUs % 1000000;

    UDT::UDSET readfds;
    UD_SET(_sock, &readfds);
    return (UDT::select(1, &readfds, NULL, NULL, &tv) == 1);
}
