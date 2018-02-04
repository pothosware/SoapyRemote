// Copyright (c) 2018-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyDNSSD.hpp"
#include <iostream>

std::shared_ptr<SoapyDNSSD> SoapyDNSSD::getInstance(void)
{
    return std::make_shared<SoapyDNSSD>();
}

SoapyDNSSD::SoapyDNSSD(void):
    _impl(nullptr)
{
    return;
}

SoapyDNSSD::~SoapyDNSSD(void)
{
    return;
}

void SoapyDNSSD::registerService(const std::string &, const std::string &)
{
    std::cerr << "SoapySDR server compiled without mDNS support" << std::endl;
}

void SoapyDNSSD::maintenance(void)
{
    return;
}

std::vector<std::string> SoapyDNSSD::getServerURLs(const int, const bool)
{
    return {};
}
