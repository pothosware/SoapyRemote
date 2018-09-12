// Copyright (c) 2018-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyIfAddrs.hpp"

SoapyIfAddr::SoapyIfAddr(void):
    ethno(0),
    ipVer(0),
    isUp(false),
    isLoopback(false),
    isMulticast(false)
{
    return;
}
