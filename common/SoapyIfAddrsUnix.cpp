// Copyright (c) 2018-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyIfAddrs.hpp"
#include "SoapySocketDefs.hpp"
#include "SoapyURLUtils.hpp"
#include <SoapySDR/Logger.hpp>

std::vector<SoapyIfAddr> listSoapyIfAddrs(void)
{
    std::vector<SoapyIfAddr> result;

    struct ifaddrs *ifaddr;
    if (getifaddrs(&ifaddr) == -1) return result;

    for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == NULL) continue;

        SoapyIfAddr ifAddr;
        switch(ifa->ifa_addr->sa_family)
        {
        case AF_INET: ifAddr.ipVer = 4; break;
        case AF_INET6: ifAddr.ipVer = 6; break;
        default: break;
        }
        if (ifAddr.ipVer == 0) continue;

        ifAddr.isUp = ((ifa->ifa_flags & IFF_UP) != 0);
        ifAddr.isLoopback = ((ifa->ifa_flags & IFF_LOOPBACK) != 0);
        ifAddr.isMulticast = ((ifa->ifa_flags & IFF_MULTICAST) != 0);
        ifAddr.ethno = if_nametoindex(ifa->ifa_name);
        ifAddr.name = ifa->ifa_name;
        ifAddr.addr = SoapyURL(ifa->ifa_addr).getNode();
        result.push_back(ifAddr);
    }

    freeifaddrs(ifaddr);
    return result;
}
