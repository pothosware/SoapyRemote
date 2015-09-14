// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include "SoapyRPCSocket.hpp"

/*!
 * Create a log forwarder to subscribe to log events from the local logger.
 */
class SoapyLogForwarder
{
public:
    SoapyLogForwarder(SoapyRPCSocket &sock);
    ~SoapyLogForwarder(void);

private:
    SoapyRPCSocket &_sock;
};
