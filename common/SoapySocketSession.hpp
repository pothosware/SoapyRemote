// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include "SoapyRemoteCommon.hpp"

/*!
 * Create one instance of the session per process to use sockets.
 */
class SoapySocketSession
{
public:
    SoapySocketSession(void);
    ~SoapySocketSession(void);
};
