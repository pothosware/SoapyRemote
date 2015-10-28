// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapySocketDefs.hpp"
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

void SoapyRPCSocket::setDefaultTcpSockOpts(void)
{
    if (this->null()) return;

    int one = 1;
    int ret = ::setsockopt(_sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof(one));
    if (ret != 0)
    {
        this->reportError("setsockopt(TCP_NODELAY)");
    }

    #ifdef TCP_QUICKACK
    ret = ::setsockopt(_sock, IPPROTO_TCP, TCP_QUICKACK, (const char *)&one, sizeof(one));
    if (ret != 0)
    {
        this->reportError("setsockopt(TCP_QUICKACK)");
    }
    #endif //TCP_QUICKACK
}

SoapyRPCSocket::SoapyRPCSocket(void):
    _sock(INVALID_SOCKET)
{
    return;
}

SoapyRPCSocket::SoapyRPCSocket(const std::string &url):
    _sock(INVALID_SOCKET)
{
    SoapyURL urlObj(url);
    SockAddrData addr;
    const auto errorMsg = urlObj.toSockAddr(addr);

    if (not errorMsg.empty())
    {
        this->reportError("getaddrinfo("+url+")", errorMsg);
    }
    else
    {
        _sock = ::socket(addr.addr()->sa_family, urlObj.getType(), 0);
    }
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
    SoapyURL urlObj(url);
    SockAddrData addr;
    const auto errorMsg = urlObj.toSockAddr(addr);
    if (not errorMsg.empty())
    {
        this->reportError("getaddrinfo("+url+")", errorMsg);
        return -1;
    }

    if (this->null()) _sock = ::socket(addr.addr()->sa_family, urlObj.getType(), 0);
    if (this->null()) return -1;

    //setup reuse address
    int one = 1;
    int ret = ::setsockopt(_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof(one));
    if (ret != 0)
    {
        this->reportError("setsockopt(SO_REUSEADDR)");
    }

    #ifdef __APPLE__
    ret = ::setsockopt(_sock, SOL_SOCKET, SO_REUSEPORT, (const char *)&one, sizeof(one));
    if (ret != 0)
    {
        this->reportError("setsockopt(SO_REUSEPORT)");
    }
    #endif //__APPLE__

    if (urlObj.getType() == SOCK_STREAM) this->setDefaultTcpSockOpts();
    return ::bind(_sock, addr.addr(), addr.addrlen());
}

int SoapyRPCSocket::listen(int backlog)
{
    return ::listen(_sock, backlog);
}

SoapyRPCSocket *SoapyRPCSocket::accept(void)
{
    struct sockaddr_storage addr;
    socklen_t addrlen = sizeof(addr);
    int client = ::accept(_sock, (struct sockaddr*)&addr, &addrlen);
    if (client == INVALID_SOCKET) return NULL;
    SoapyRPCSocket *clientSock = new SoapyRPCSocket();
    clientSock->_sock = client;
    clientSock->setDefaultTcpSockOpts();
    return clientSock;
}

int SoapyRPCSocket::connect(const std::string &url)
{
    SoapyURL urlObj(url);
    SockAddrData addr;
    const auto errorMsg = urlObj.toSockAddr(addr);
    if (not errorMsg.empty())
    {
        this->reportError("getaddrinfo("+url+")", errorMsg);
        return -1;
    }

    if (this->null()) _sock = ::socket(addr.addr()->sa_family, urlObj.getType(), 0);
    if (this->null()) return -1;
    if (urlObj.getType() == SOCK_STREAM) this->setDefaultTcpSockOpts();
    return ::connect(_sock, addr.addr(), addr.addrlen());
}

int SoapyRPCSocket::multicastJoin(const std::string &group, const bool loop, const int ttl)
{
    /*
     * Multicast join docs:
     * http://www.tldp.org/HOWTO/Multicast-HOWTO-6.html
     * http://www.tenouk.com/Module41c.html
     */

    //lookup group url
    SoapyURL urlObj(group);
    SockAddrData addr;
    const auto errorMsg = urlObj.toSockAddr(addr);
    if (not errorMsg.empty())
    {
        this->reportError("getaddrinfo("+group+")", errorMsg);
        return -1;
    }

    //create socket if null
    if (this->null()) _sock = ::socket(addr.addr()->sa_family, SOCK_DGRAM, 0);
    if (this->null()) return -1;
    int ret = 0;

    int loopInt = loop?1:0;

    switch(addr.addr()->sa_family)
    {
    case AF_INET: {

        //setup IP_MULTICAST_LOOP
        ret = ::setsockopt(_sock, IPPROTO_IP, IP_MULTICAST_LOOP, (const char *)&loopInt, sizeof(loopInt));
        if (ret != 0)
        {
            this->reportError("setsockopt(IP_MULTICAST_LOOP)");
            return -1;
        }

        //setup IP_MULTICAST_TTL
        ret = ::setsockopt(_sock, IPPROTO_IP, IP_MULTICAST_TTL, (const char *)&ttl, sizeof(ttl));
        if (ret != 0)
        {
            this->reportError("setsockopt(IP_MULTICAST_TTL)");
            return -1;
        }

        //setup IP_ADD_MEMBERSHIP
        auto *addr_in = (const struct sockaddr_in *)addr.addr();
        struct ip_mreq mreq;
        mreq.imr_multiaddr = addr_in->sin_addr;
        mreq.imr_interface.s_addr = INADDR_ANY;
        ret = ::setsockopt(_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char *)&mreq, sizeof(mreq));
        if (ret != 0)
        {
            this->reportError("setsockopt(IP_ADD_MEMBERSHIP)");
            return -1;
        }
        break;
    }
    case AF_INET6: {

        //setup IPV6_MULTICAST_LOOP
        ret = ::setsockopt(_sock, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, (const char *)&loopInt, sizeof(loopInt));
        if (ret != 0)
        {
            this->reportError("setsockopt(IPV6_MULTICAST_LOOP)");
            return -1;
        }

        //setup IPV6_MULTICAST_HOPS
        ret = ::setsockopt(_sock, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, (const char *)&ttl, sizeof(ttl));
        if (ret != 0)
        {
            this->reportError("setsockopt(IPV6_MULTICAST_HOPS)");
            return -1;
        }

        //setup IPV6_ADD_MEMBERSHIP
        auto *addr_in6 = (const struct sockaddr_in6 *)addr.addr();
        struct ipv6_mreq mreq6;
        mreq6.ipv6mr_multiaddr = addr_in6->sin6_addr;
        mreq6.ipv6mr_interface = 0;//local_addr_in6->sin6_scope_id;
        ret = ::setsockopt(_sock, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, (const char *)&mreq6, sizeof(mreq6));
        if (ret != 0)
        {
            this->reportError("setsockopt(IPV6_ADD_MEMBERSHIP)");
            return -1;
        }
        break;
    }
    default:
        break;
    }

    return 0;
}

int SoapyRPCSocket::send(const void *buf, size_t len, int flags)
{
    return ::send(_sock, (const char *)buf, int(len), flags);
}

int SoapyRPCSocket::recv(void *buf, size_t len, int flags)
{
    return ::recv(_sock, (char *)buf, int(len), flags);
}

int SoapyRPCSocket::sendto(const void *buf, size_t len, const std::string &url, int flags)
{
    SockAddrData addr; SoapyURL(url).toSockAddr(addr);
    return ::sendto(_sock, (char *)buf, int(len), flags, addr.addr(), addr.addrlen());
}

int SoapyRPCSocket::recvfrom(void *buf, size_t len, std::string &url, int flags)
{
    struct sockaddr_storage addr;
    socklen_t addrlen = sizeof(addr);
    int ret = ::recvfrom(_sock, (char *)buf, int(len), flags, (struct sockaddr*)&addr, &addrlen);
    url = SoapyURL(SockAddrData((struct sockaddr *)&addr, addrlen)).toString();
    return ret;
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

static std::string errToString(const int err)
{
    char buff[1024];
    #ifdef _MSC_VER
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&buff, sizeof(buff), NULL);
    return buff;
    #else
    //http://linux.die.net/man/3/strerror_r
    #if ((_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! _GNU_SOURCE) || __APPLE__
    strerror_r(errno, buff, sizeof(buff));
    #else
    //this version may decide to use its own internal string
    return strerror_r(err, buff, sizeof(buff));
    #endif
    return buff;
    #endif
}

void SoapyRPCSocket::reportError(const std::string &what)
{
    const int err = SOCKET_ERRNO;
    if (err == 0) _lastErrorMsg = what;
    else this->reportError(what, std::to_string(err) + ": " + errToString(err));
}

void SoapyRPCSocket::reportError(const std::string &what, const std::string &errorMsg)
{
    _lastErrorMsg = what + " [" + errorMsg + "]";
}

std::string SoapyRPCSocket::getsockname(void)
{
    struct sockaddr_storage addr;
    socklen_t addrlen = sizeof(addr);
    int ret = ::getsockname(_sock, (struct sockaddr *)&addr, &addrlen);
    if (ret != 0) return "";
    return SoapyURL(SockAddrData((struct sockaddr *)&addr, addrlen)).toString();
}

std::string SoapyRPCSocket::getpeername(void)
{
    struct sockaddr_storage addr;
    socklen_t addrlen = sizeof(addr);
    int ret = ::getpeername(_sock, (struct sockaddr *)&addr, &addrlen);
    if (ret != 0) return "";
    return SoapyURL(SockAddrData((struct sockaddr *)&addr, addrlen)).toString();
}

static int setBuffSizeHelper(const int sock, int level, int option_name, const size_t numBytes)
{
    int opt = int(numBytes);
    int ret = ::setsockopt(sock, level, option_name, (const char *)&opt, sizeof(opt));
    if (ret != 0) return ret;

    socklen_t optlen = sizeof(opt);
    ret = ::getsockopt(sock, level, option_name, (char *)&opt, &optlen);
    if (ret != 0) return ret;

    //adjustment for linux kernel socket buffer doubling for bookkeeping
    #ifdef __linux
    opt = opt/2;
    #endif

    return opt;
}

int SoapyRPCSocket::setRecvBuffSize(const size_t numBytes)
{
    return setBuffSizeHelper(_sock, SOL_SOCKET, SO_RCVBUF, numBytes);
}

int SoapyRPCSocket::setSendBuffSize(const size_t numBytes)
{
    return setBuffSizeHelper(_sock, SOL_SOCKET, SO_SNDBUF, numBytes);
}
