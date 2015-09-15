// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyNetworkDefs.hpp"
#include "SoapyRPCSocket.hpp"
#include "SoapyURLUtils.hpp"
#include <SoapySDR/Logger.hpp>
#include <cstring> //strerror
#include <cerrno> //errno
#include <mutex>

static std::mutex sessionMutex;
static size_t sessionCount = 0;

SoapySocketSession::SoapySocketSession(void)
{
    std::lock_guard<std::mutex> lock(sessionMutex);
    sessionCount++;
    if (sessionCount > 1) return;

    #ifdef _MSC_VER
    WORD wVersionRequested;
    WSADATA wsaData;
    wVersionRequested = MAKEWORD(2, 2);
    int ret = WSAStartup(wVersionRequested, &wsaData);
    if (ret != 0)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "SoapySocketSession::WSAStartup: %d", ret);
    }
    #endif
}

SoapySocketSession::~SoapySocketSession(void)
{
    std::lock_guard<std::mutex> lock(sessionMutex);
    sessionCount--;
    if (sessionCount > 0) return;

    #ifdef _MSC_VER
    WSACleanup();
    #endif
}

static void defaultTcpSockOpts(int sock)
{
    if (sock == INVALID_SOCKET) return;

    int one = 1;
    int ret = ::setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof(one));
    if (ret != 0)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "SoapySocketSession::setsockopt(TCP_NODELAY) -- %d", ret);
    }

    #ifdef TCP_QUICKACK
    ret = ::setsockopt(sock, IPPROTO_TCP, TCP_QUICKACK, (const char *)&one, sizeof(one));
    if (ret != 0)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "SoapySocketSession::setsockopt(TCP_QUICKACK) -- %d", ret);
    }
    #endif //TCP_QUICKACK

    ret = ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof(one));
    if (ret != 0)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "SoapySocketSession::setsockopt(SO_REUSEADDR) -- %d", ret);
    }
}

SoapyRPCSocket::SoapyRPCSocket(void):
    _sock(INVALID_SOCKET)
{
    return;
}

SoapyRPCSocket::~SoapyRPCSocket(void)
{
    if (this->close() != 0)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "SoapyRPCSocket::~SoapyRPCSocket: %s", this->lastErrorMsg());
    }
}

bool SoapyRPCSocket::null(void)
{
    return _sock == INVALID_SOCKET;
}

int SoapyRPCSocket::close(void)
{
    if (this->null()) return 0;
    int ret = ::closesocket(_sock);
    _sock = INVALID_SOCKET;
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
        return -1;
    }

    _sock = ::socket(af, type, prot);
    if (this->null()) return -1;
    if (af == SOCK_STREAM) defaultTcpSockOpts(_sock);
    return ::bind(_sock, &addr, addrlen);
}

int SoapyRPCSocket::listen(int backlog)
{
    return ::listen(_sock, backlog);
}

SoapyRPCSocket *SoapyRPCSocket::accept(void)
{
    struct sockaddr addr;
    socklen_t addrlen = sizeof(addr);
    int client = ::accept(_sock, &addr, &addrlen);
    if (client == INVALID_SOCKET) return NULL;
    defaultTcpSockOpts(client);
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
        return -1;
    }

    _sock = ::socket(af, type, prot);
    if (this->null()) return -1;
    if (af == SOCK_STREAM) defaultTcpSockOpts(_sock);
    return ::connect(_sock, &addr, addrlen);
}

int SoapyRPCSocket::send(const void *buf, size_t len, int flags)
{
    return ::send(_sock, (const char *)buf, int(len), flags);
}

int SoapyRPCSocket::recv(void *buf, size_t len, int flags)
{
    return ::recv(_sock, (char *)buf, int(len), flags);
}

bool SoapyRPCSocket::selectRecv(const long timeoutUs)
{
    struct timeval tv;
    tv.tv_sec = timeoutUs / 1000000;
    tv.tv_usec = timeoutUs % 1000000;

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(_sock, &readfds);
    return ::select(_sock+1, &readfds, NULL, NULL, &tv) == 1;
}

const char *SoapyRPCSocket::lastErrorMsg(void)
{
    #ifdef _MSC_VER
    static __declspec(thread) char buff[1024];
    int err = WSAGetLastError();
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&buff, sizeof(buff), NULL);
    return buff;
    #else
    static __thread char buff[1024];
    strerror_r(errno, buff, sizeof(buff));
    return buff;
    #endif
}

std::string SoapyRPCSocket::getsockname(void)
{
    struct sockaddr addr;
    socklen_t addrlen = sizeof(addr);
    int ret = ::getsockname(_sock, &addr, &addrlen);
    if (ret != 0) return "";
    return sockaddrToURL(addr);
}

std::string SoapyRPCSocket::getpeername(void)
{
    struct sockaddr addr;
    socklen_t addrlen = sizeof(addr);
    int ret = ::getpeername(_sock, &addr, &addrlen);
    if (ret != 0) return "";
    return sockaddrToURL(addr);
}

int SoapyRPCSocket::setRecvBuffSize(const size_t numBytes)
{
    int opt = int(numBytes);
    int ret = setsockopt(_sock, SOL_SOCKET, SO_RCVBUF, &opt, sizeof(opt));
    if (ret != 0) return ret;

    socklen_t optlen = sizeof(opt);
    ret = getsockopt(_sock, SOL_SOCKET, SO_RCVBUF, &opt, &optlen);
    if (ret != 0) return ret;

    return opt;
}

int SoapyRPCSocket::setSendBuffSize(const size_t numBytes)
{
    int opt = int(numBytes);
    int ret = setsockopt(_sock, SOL_SOCKET, SO_SNDBUF, &opt, sizeof(opt));
    if (ret != 0) return ret;

    socklen_t optlen = sizeof(opt);
    ret = getsockopt(_sock, SOL_SOCKET, SO_SNDBUF, &opt, &optlen);
    if (ret != 0) return ret;

    return opt;
}
