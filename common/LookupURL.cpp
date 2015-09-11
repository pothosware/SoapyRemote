// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyNetworkDefs.hpp"
#include "SoapyRemoteCommon.hpp"
#include <cstring> //memset
#include <string>

bool lookupURL(const std::string &url,
    int &af, int &type, int &prot,
    struct sockaddr &addr, int &addrlen,
    std::string &errorMsg)
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
    int ret = getaddrinfo(node.c_str(), service.c_str(), &hints, &servinfo);
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
