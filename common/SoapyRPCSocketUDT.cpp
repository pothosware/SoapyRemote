// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyNetworkDefs.hpp"
#include "SoapyRPCSocket.hpp"
#include <SoapySDR/Logger.hpp>
#include <udt.h>

SoapySocketSession::SoapySocketSession(void)
{
    if (UDT::startup() != 0)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "SoapySocketSession::SoapySocketSession: %s", UDT::getlasterror().getErrorMessage());
    }
}

SoapySocketSession::~SoapySocketSession(void)
{
    if (UDT::cleanup() != 0)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "SoapySocketSession::~SoapySocketSession: %s", UDT::getlasterror().getErrorMessage());
    }
}

static void defaultSockOpts(int sock)
{
    if (sock == UDT::INVALID_SOCK) return;
    //Arbitrary buffer size limit in OSX that must be set on the socket,
    //or UDT will try to set one that is too large and fail bind/connect.
    #ifdef HAS_APPLE_OS
    int size = 1024*21;
    UDT::setsockopt(sock, 0, UDP_RCVBUF, &size, int(sizeof(size)));
    #endif
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
        SoapySDR::logf(SOAPY_SDR_ERROR, "SoapyRPCSocket::~SoapyRPCSocket: %s", UDT::getlasterror().getErrorMessage());
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
        SoapySDR::logf(SOAPY_SDR_ERROR, "SoapyRPCSocket::bind(%s): %s", url.c_str(), errorMsg.c_str());
        return UDT::ERROR;
    }

    _sock = UDT::socket(af, type, prot);
    if (this->null()) return UDT::ERROR;
    defaultSockOpts(_sock);
    return UDT::bind(_sock, &addr, addrlen);
}

int SoapyRPCSocket::listen(int backlog)
{
    return UDT::listen(_sock, backlog);
}

SoapyRPCSocket *SoapyRPCSocket::accept(void)
{
    struct sockaddr addr;
    int addrlen = sizeof(addr);
    int client = UDT::accept(_sock, &addr, &addrlen);
    if (client == UDT::INVALID_SOCK) return NULL;
    defaultSockOpts(client);
    SoapyRPCSocket *clientSock = new SoapyRPCSocket();
    clientSock->_sock = client;
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
        SoapySDR::logf(SOAPY_SDR_ERROR, "SoapyRPCSocket::connect(%s): %s", url.c_str(), errorMsg.c_str());
        return UDT::ERROR;
    }

    _sock = UDT::socket(af, type, prot);
    if (this->null()) return UDT::ERROR;
    defaultSockOpts(_sock);
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

const char *SoapyRPCSocket::lastErrorMsg(void)
{
    return UDT::getlasterror().getErrorMessage();
}

std::string SoapyRPCSocket::getsockname(void)
{
    struct sockaddr addr;
    int addrlen = sizeof(addr);
    int ret = UDT::getsockname(_sock, &addr, &addrlen);
    if (ret != 0) return this->lastErrorMsg();
    return sockaddrToURL(addr);
}

std::string SoapyRPCSocket::getpeername(void)
{
    struct sockaddr addr;
    int addrlen = sizeof(addr);
    int ret = UDT::getpeername(_sock, &addr, &addrlen);
    if (ret != 0) return this->lastErrorMsg();
    return sockaddrToURL(addr);
}
