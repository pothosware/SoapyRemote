// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapySocketDefs.hpp"
#include "SoapyURLUtils.hpp"
#include "SoapyRemoteDefs.hpp" //default port
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

bool splitURL(
    const std::string &url,
    std::string &scheme,
    std::string &node,
    std::string &service
)
{
    scheme.clear();
    node.clear();
    service.clear();

    //extract the scheme
    std::string urlRest = url;
    auto schemeEnd = url.find("://");
    if (schemeEnd != std::string::npos)
    {
        scheme = url.substr(0, schemeEnd);
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

    if (node.empty()) return false;
    return true;
}

std::string combineURL(
    const std::string &scheme,
    const std::string &node,
    const std::string &service)
{
    std::string url;

    //add the scheme
    if (not scheme.empty()) url += scheme + "://";

    //add the node with ipv6 escape brackets
    if (node.find(":") != std::string::npos) url += "[" + node + "]";
    else url += node;

    //and the service
    if (not service.empty()) url += ":" + service;

    return url;
}

std::string lookupURL(const std::string &url, SockAddrData &addr)
{
    //parse the url into the node and service
    std::string scheme, node, service;
    if (not splitURL(url, scheme, node, service)) return "url parse failed";

    //unspecified service, select default port
    if (service.empty()) return "service not specified";

    //configure the hint
    struct addrinfo hints, *servinfo = NULL;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
    hints.ai_socktype = SOCK_STREAM;

    //get address info
    int ret = getaddrinfo(node.c_str(), service.c_str(), &hints, &servinfo);
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

    return (p == NULL)?"no lookup results":"";
}

std::string sockaddrToURL(const SockAddrData &addr)
{
    std::string url;

    char *s = NULL;
    switch(addr.addr()->sa_family)
    {
        case AF_INET: {
            struct sockaddr_in *addr_in = (struct sockaddr_in *)addr.addr();
            s = (char *)malloc(INET_ADDRSTRLEN);
            inet_ntop(AF_INET, &(addr_in->sin_addr), s, INET_ADDRSTRLEN);
            url = combineURL("", std::string(s), std::to_string(ntohs(addr_in->sin_port)));
            break;
        }
        case AF_INET6: {
            struct sockaddr_in6 *addr_in6 = (struct sockaddr_in6 *)addr.addr();
            s = (char *)malloc(INET6_ADDRSTRLEN);
            inet_ntop(AF_INET6, &(addr_in6->sin6_addr), s, INET6_ADDRSTRLEN);
            url = combineURL("", std::string(s),  std::to_string(ntohs(addr_in6->sin6_port)));
            break;
        }
        default:
            break;
    }
    free(s);

    return url;
}

#ifdef _MSC_VER

#define getpid() GetCurrentProcessId()

static DWORD gethostid(void)
{
    char szVolName[MAX_PATH];
    char szFileSysName[80];
    DWORD dwSerialNumber;
    DWORD dwMaxComponentLen;
    DWORD dwFileSysFlags;
    GetVolumeInformation(
        "C:\\", szVolName, MAX_PATH,
        &dwSerialNumber, &dwMaxComponentLen,
        &dwFileSysFlags, szFileSysName, sizeof(szFileSysName));
    return dwSerialNumber;
}

#endif //_MSC_VER

std::string uniqueProcessId(void)
{
    //get hostname
    std::string hostname;
    char hostnameBuff[128];
    int ret = gethostname(hostnameBuff, sizeof(hostnameBuff));
    if (ret == 0) hostname = std::string(hostnameBuff);
    else hostname = "unknown";

    //get host and process ids
    int pid = getpid();
    int hid = gethostid();

    //combine for unique process ID
    return "upid://" + hostname + "?pid=" + std::to_string(pid) + "&hid=" + std::to_string(hid);
}
