// Copyright (c) 2018-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyMDNSEndpoint.hpp"
#include <SoapySDR/Logger.hpp>

SoapyMDNSEndpoint::SoapyMDNSEndpoint(void):
    _impl(nullptr)
{
    return;
}

SoapyMDNSEndpoint::~SoapyMDNSEndpoint(void)
{
    return;
}

void SoapyMDNSEndpoint::printInfo(void)
{
    //we usually can build avahi support on linux, so only warn on linux
    #ifdef __linux__
    SoapySDR::log(SOAPY_SDR_WARNING, "SoapyRemote compiled without DNS-SD support!");
    #endif //__linux__
}

bool SoapyMDNSEndpoint::status(void)
{
    return true;
}

void SoapyMDNSEndpoint::registerService(const std::string &, const std::string &, const int)
{
    return;
}

std::map<std::string, std::map<int, std::string>> SoapyMDNSEndpoint::getServerURLs(const int, const long)
{
    return {};
}
