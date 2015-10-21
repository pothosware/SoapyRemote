// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapySocketDefs.hpp"
#include "SoapyURLUtils.hpp"
#include <cstring> //memset
#include <string>
#include <cassert>

SockAddrData::SockAddrData(void)
{
    return;
}

SockAddrData::SockAddrData(const struct sockaddr *addr, const int addrlen)
{
    _storage.resize(addrlen);
    std::memcpy(_storage.data(), addr, addrlen);
}

const struct sockaddr *SockAddrData::addr(void) const
{
    return (const struct sockaddr *)_storage.data();
}

size_t SockAddrData::addrlen(void) const
{
    return _storage.size();
}

SoapyURL::SoapyURL(void)
{
    return;
}

SoapyURL::SoapyURL(const std::string &scheme, const std::string &node, const std::string &service):
    _scheme(scheme),
    _node(node),
    _service(service)
{
    return;
}

SoapyURL::SoapyURL(const std::string &url)
{
    //extract the scheme
    std::string urlRest = url;
    auto schemeEnd = url.find("://");
    if (schemeEnd != std::string::npos)
    {
        _scheme = url.substr(0, schemeEnd);
        urlRest = url.substr(schemeEnd+3);
    }

    //extract node name and service port
    bool inBracket = false;
    bool inService = false;
    for (size_t i = 0; i < urlRest.size(); i++)
    {
        const char ch = urlRest[i];
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
            _node += ch;
            continue;
        }
        if (inService)
        {
            _service += ch;
            continue;
        }
        if (not inService and ch == ':')
        {
            inService = true;
            continue;
        }
        if (not inService)
        {
            _node += ch;
            continue;
        }
    }
}

SoapyURL::SoapyURL(const SockAddrData &addr)
{
    char *s = NULL;
    switch(addr.addr()->sa_family)
    {
        case AF_INET: {
            auto *addr_in = (const struct sockaddr_in *)addr.addr();
            s = (char *)malloc(INET_ADDRSTRLEN);
            inet_ntop(AF_INET, (void *)&(addr_in->sin_addr), s, INET_ADDRSTRLEN);
            _node = s;
            _service = std::to_string(ntohs(addr_in->sin_port));
            break;
        }
        case AF_INET6: {
            auto *addr_in6 = (const struct sockaddr_in6 *)addr.addr();
            s = (char *)malloc(INET6_ADDRSTRLEN);
            inet_ntop(AF_INET6, (void *)&(addr_in6->sin6_addr), s, INET6_ADDRSTRLEN);
            _node = s;
            //support scoped address node
            if (addr_in6->sin6_scope_id != 0)
            {
                _node += "%" + std::to_string(addr_in6->sin6_scope_id);
            }
            _service = std::to_string(ntohs(addr_in6->sin6_port));
            break;
        }
        default:
            break;
    }
    free(s);
}

std::string SoapyURL::toSockAddr(SockAddrData &addr) const
{
    SockAddrData result;

    //unspecified service, cant continue
    if (_service.empty()) return "service not specified";

    //configure the hint
    struct addrinfo hints, *servinfo = NULL;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
    hints.ai_socktype = this->getType();

    //get address info
    int ret = getaddrinfo(_node.c_str(), _service.c_str(), &hints, &servinfo);
    if (ret != 0) return gai_strerror(ret);

    //iterate through possible matches
    struct addrinfo *p = NULL;
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        //eliminate unsupported family types
        if (p->ai_family != AF_INET and p->ai_family != AF_INET6) continue;

        //found a match
        assert(p->ai_family == p->ai_addr->sa_family);
        addr = SockAddrData(p->ai_addr, p->ai_addrlen);
        break;
    }

    //cleanup
    freeaddrinfo(servinfo);

    //no results
    if (p == NULL) return "no lookup results";

    return ""; //OK
}

std::string SoapyURL::toString(void) const
{
    std::string url;

    //add the scheme
    if (not _scheme.empty()) url += _scheme + "://";

    //add the node with ipv6 escape brackets
    if (_node.find(":") != std::string::npos) url += "[" + _node + "]";
    else url += _node;

    //and the service
    if (not _service.empty()) url += ":" + _service;

    return url;
}

std::string SoapyURL::getScheme(void) const
{
    return _scheme;
}

std::string SoapyURL::getNode(void) const
{
    return _node;
}

std::string SoapyURL::getService(void) const
{
    return _service;
}

void SoapyURL::setScheme(const std::string &scheme)
{
    _scheme = scheme;
}

void SoapyURL::setNode(const std::string &node)
{
    _node = node;
}

void SoapyURL::setService(const std::string &service)
{
    _service = service;
}

int SoapyURL::getType(void) const
{
    if (_scheme == "tcp") return SOCK_STREAM;
    if (_scheme == "udp") return SOCK_DGRAM;
    return SOCK_STREAM; //assume
}
