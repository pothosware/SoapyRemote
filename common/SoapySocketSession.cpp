// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapySocketSession.hpp"
#include <iostream>
#include <udt.h>

SoapySocketSession::SoapySocketSession(void)
{
    if (UDT::startup() != 0)
    {
        std::cerr << "SoapySocketSession::SoapySocketSession: " << UDT::getlasterror().getErrorMessage() << std::endl;
    }
}

SoapySocketSession::~SoapySocketSession(void)
{
    if (UDT::cleanup() != 0)
    {
        std::cerr << "SoapySocketSession::~SoapySocketSession: " << UDT::getlasterror().getErrorMessage() << std::endl;
    }
}
