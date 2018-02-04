// Copyright (c) 2018-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyDNSSD.hpp"
#include <SoapySDR/Logger.hpp>

SoapyDNSSD::SoapyDNSSD(void):
    _impl(nullptr)
{
    return;
}

SoapyDNSSD::~SoapyDNSSD(void)
{
    return;
}

void SoapyDNSSD::printInfo(void)
{
    //we usually can build avahi support on linux, so only warn on linux
    #ifdef __linux__
    SoapySDR::log(SOAPY_SDR_WARNING, "SoapyRemote compiled without DNS-SD support!");
    #endif //__linux__
}

void SoapyDNSSD::registerService(const std::string &, const std::string &, const int)
{
    return;
}

std::map<std::string, std::map<int, std::string>> SoapyDNSSD::getServerURLs(const int)
{
    return {};
}
