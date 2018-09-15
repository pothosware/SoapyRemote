// Copyright (c) 2018-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include "SoapyRemoteConfig.hpp"
#include <string>
#include <vector>

struct SoapyIfAddr
{
    SoapyIfAddr(void);
    int ethno; //! The ethernet index
    int ipVer; //! The ip protocol: 4 or 6
    bool isUp; //! Is this link active?
    bool isLoopback; //! Is this a loopback interface?
    bool isMulticast; //! Does this interface support multicast?
    std::string name; //! The interface name: ex eth0
    std::string addr; //! The ip address as a string
};

//! Get a list of IF addrs
SOAPY_REMOTE_API std::vector<SoapyIfAddr> listSoapyIfAddrs(void);
