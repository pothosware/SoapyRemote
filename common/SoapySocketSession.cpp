// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapySocketSession.hpp"
#include <SoapySDR/Logger.hpp>
#include <udt.h>

SoapySocketSession::SoapySocketSession(void)
{
    if (UDT::startup() != 0)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "SoapySocketSession::SoapySocketSession: %s", UDT::getlasterror().getErrorMessage());
    }
}

SoapySocketSession::~SoapySocketSession(void)
{
    if (UDT::cleanup() != 0)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "SoapySocketSession::~SoapySocketSession: %s", UDT::getlasterror().getErrorMessage());
    }
}
