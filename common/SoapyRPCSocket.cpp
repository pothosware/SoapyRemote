// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyNetworkDefs.hpp"
#include "SoapyRPCSocket.hpp"
#include <SoapySDR/Logger.hpp>
#include <udt.h>

bool lookupURL(const std::string &url,
    int &af, int &type, int &prot,
    struct sockaddr &addr, int &addrlen,
    std::string &errorMsg);

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
        SoapySDR::logf(SOAPY_SDR_ERROR, "SoapyRPCSocket::connect(%s): %s", url.c_str(), errorMsg.c_str());
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
