// Copyright (c) 2018-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "DNSSDPublish.hpp"
#include <iostream>

DNSSDPublish::DNSSDPublish(void):
    _impl(nullptr)
{
    std::cerr << "SoapySDR server compiled without mDNS support" << std::endl;
}

DNSSDPublish::~DNSSDPublish(void)
{
    return;
}

void DNSSDPublish::maintenance(void)
{
    return;
}
