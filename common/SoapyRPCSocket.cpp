// Copyright (c) 2015-2018 Josh Blum
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

bool SoapyRPCSocket::status(void)
{
    int opt = 0;
    socklen_t optlen = sizeof(opt);
    ::getsockopt(_sock, SOL_SOCKET, SO_ERROR, (char *)&opt, &optlen);
    if (opt != 0) this->reportError("getsockopt(SO_ERROR)", opt);
    return opt == 0;
}

int SoapyRPCSocket::close(void)
{
    if (this->null()) return 0;
    int ret = ::closesocket(_sock);
    _sock = INVALID_SOCKET;
    if (ret != 0) this->reportError("closesocket()");
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

    ret = ::bind(_sock, addr.addr(), addr.addrlen());
    if (ret == -1) this->reportError("bind("+url+")");
    return ret;
}

int SoapyRPCSocket::listen(int backlog)
{
    int ret = ::listen(_sock, backlog);
    if (ret == -1) this->reportError("listen()");
    return ret;
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

    int ret = ::connect(_sock, addr.addr(), addr.addrlen());
    if (ret == -1) this->reportError("connect("+url+")");
    return ret;
}

int SoapyRPCSocket::connect(const std::string &url, const long timeoutUs)
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

    //enable non blocking
    int ret = this->setNonBlocking(true);
    if (ret != 0) return ret;

    //non blocking connect, check for non busy
    ret = ::connect(_sock, addr.addr(), addr.addrlen());
    if (ret != 0 and SOCKET_ERRNO != SOCKET_EINPROGRESS)
    {
        this->reportError("connect("+url+")");
        return ret;
    }

    //fill in the select structures
    struct timeval tv;
    tv.tv_sec = timeoutUs / 1000000;
    tv.tv_usec = timeoutUs % 1000000;

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(_sock, &fds);

    //wait for connect or timeout
    ret = ::select(_sock+1, NULL, &fds, NULL, &tv);
    if (ret != 1)
    {
        this->reportError("connect("+url+")", SOCKET_ETIMEDOUT);
        return -1;
    }

    //get the error code from connect()
    int opt = 0;
    socklen_t optlen = sizeof(opt);
    ::getsockopt(_sock, SOL_SOCKET, SO_ERROR, (char *)&opt, &optlen);
    if (opt != 0)
    {
        this->reportError("connect("+url+")", opt);
        return opt;
    }

    //revert non blocking on socket
    ret = this->setNonBlocking(false);
    if (ret != 0) return ret;

    return opt;
}

int SoapyRPCSocket::setNonBlocking(const bool nonblock)
{
    int ret = 0;
    #ifdef _MSC_VER
    u_long mode = nonblock?1:0;  // 1 to enable non-blocking socket
    ret = ioctlsocket(_sock, FIONBIO, &mode);
    #else
    int mode = fcntl(_sock, F_GETFL, 0);
    if (nonblock) mode |= O_NONBLOCK;
    else mode &= ~(O_NONBLOCK);
    ret = fcntl(_sock, F_SETFL, mode);
    #endif
    if (ret != 0) this->reportError("setNonBlocking("+std::string(nonblock?"true":"false")+")");
    return ret;
}

/*!
 * OSX doesn't support automatic ipv6mr_interface = 0.
 * The following code attempts to work around this issue
 * by manually selecting a multicast capable interface.
 */
static int getDefaultIfaceIndex(void)
{
    #ifdef __APPLE__

    //find the first available multicast interfaces
    int loIface = 0, enIface = 0;
    struct ifaddrs *ifa = nullptr;
    getifaddrs(&ifa);
    while (ifa != nullptr)
    {
        const bool isIPv6 = ifa->ifa_addr->sa_family == AF_INET6;
        const bool isUp = ((ifa->ifa_flags & IFF_UP) != 0);
        const bool isLoopback = ((ifa->ifa_flags & IFF_LOOPBACK) != 0);
        const bool isMulticast = ((ifa->ifa_flags & IFF_MULTICAST) != 0);
        const int ifaceIndex = if_nametoindex(ifa->ifa_name);
        SoapySDR::logf(SOAPY_SDR_DEBUG, "Interface: #%d(%s) ipv6=%d, up=%d, lb=%d, mcast=%d",
            ifaceIndex, ifa->ifa_name, isIPv6, isUp, isLoopback, isMulticast);
        if (isIPv6 and isUp and isLoopback and isMulticast and loIface == 0) loIface = ifaceIndex;
        if (isIPv6 and isUp and not isLoopback and isMulticast and enIface == 0) enIface = ifaceIndex;
        ifa = ifa->ifa_next;
    }
    freeifaddrs(ifa);
    SoapySDR::logf(SOAPY_SDR_DEBUG, "Default loopback: #%d, default ethernet #%d", loIface, enIface);

    //prefer discovered regular interface over loopback
    if (enIface != 0) return enIface;
    if (loIface != 0) return loIface;
    #endif //__APPLE__

    return 0;
}

int SoapyRPCSocket::multicastJoin(const std::string &group, const bool loop, const int ttl, int iface)
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

        //setup IPV6_MULTICAST_IF
        if (iface == 0) iface = getDefaultIfaceIndex();
        if (iface != 0)
        {
            ret = ::setsockopt(_sock, IPPROTO_IPV6, IPV6_MULTICAST_IF, (const char *)&iface, sizeof(iface));
            if (ret != 0)
            {
                this->reportError("setsockopt(IPV6_MULTICAST_IF)");
                return -1;
            }
        }

        //setup IPV6_ADD_MEMBERSHIP
        auto *addr_in6 = (const struct sockaddr_in6 *)addr.addr();
        struct ipv6_mreq mreq6;
        mreq6.ipv6mr_multiaddr = addr_in6->sin6_addr;
        mreq6.ipv6mr_interface = iface;
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
    int ret = ::send(_sock, (const char *)buf, int(len), flags | MSG_NOSIGNAL);
    if (ret == -1) this->reportError("send()");
    return ret;
}

int SoapyRPCSocket::recv(void *buf, size_t len, int flags)
{
    int ret = ::recv(_sock, (char *)buf, int(len), flags);
    if (ret == -1) this->reportError("recv()");
    return ret;
}

int SoapyRPCSocket::sendto(const void *buf, size_t len, const std::string &url, int flags)
{
    SockAddrData addr; SoapyURL(url).toSockAddr(addr);
    int ret = ::sendto(_sock, (char *)buf, int(len), flags, addr.addr(), addr.addrlen());
    if (ret == -1) this->reportError("sendto("+url+")");
    return ret;
}

int SoapyRPCSocket::recvfrom(void *buf, size_t len, std::string &url, int flags)
{
    struct sockaddr_storage addr;
    socklen_t addrlen = sizeof(addr);
    int ret = ::recvfrom(_sock, (char *)buf, int(len), flags, (struct sockaddr*)&addr, &addrlen);
    if (ret == -1) this->reportError("recvfrom()");
    else url = SoapyURL((struct sockaddr *)&addr).toString();
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

    int ret = ::select(_sock+1, &readfds, NULL, NULL, &tv);
    if (ret == -1) this->reportError("select()");
    return ret == 1;
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
    strerror_r(err, buff, sizeof(buff));
    #else
    //this version may decide to use its own internal string
    return strerror_r(err, buff, sizeof(buff));
    #endif
    return buff;
    #endif
}

void SoapyRPCSocket::reportError(const std::string &what)
{
    this->reportError(what, SOCKET_ERRNO);
}

void SoapyRPCSocket::reportError(const std::string &what, const int err)
{
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
    if (ret == -1) this->reportError("getsockname()");
    if (ret != 0) return "";
    return SoapyURL((struct sockaddr *)&addr).toString();
}

std::string SoapyRPCSocket::getpeername(void)
{
    struct sockaddr_storage addr;
    socklen_t addrlen = sizeof(addr);
    int ret = ::getpeername(_sock, (struct sockaddr *)&addr, &addrlen);
    if (ret == -1) this->reportError("getpeername()");
    if (ret != 0) return "";
    return SoapyURL((struct sockaddr *)&addr).toString();
}

int SoapyRPCSocket::setBuffSize(const bool isRecv, const size_t numBytes)
{
    int opt = int(numBytes);
    int ret = ::setsockopt(_sock, SOL_SOCKET, isRecv?SO_RCVBUF:SO_SNDBUF, (const char *)&opt, sizeof(opt));
    if (ret == -1) this->reportError("setsockopt("+std::string(isRecv?"SO_RCVBUF":"SO_SNDBUF")+")");
    return ret;
}

int SoapyRPCSocket::getBuffSize(const bool isRecv)
{
    int opt = 0;
    socklen_t optlen = sizeof(opt);
    int ret = ::getsockopt(_sock, SOL_SOCKET, isRecv?SO_RCVBUF:SO_SNDBUF, (char *)&opt, &optlen);
    if (ret == -1) this->reportError("getsockopt("+std::string(isRecv?"SO_RCVBUF":"SO_SNDBUF")+")");
    if (ret != 0) return ret;

    //adjustment for linux kernel socket buffer doubling for bookkeeping
    #ifdef __linux
    opt = opt/2;
    #endif

    return opt;
}
