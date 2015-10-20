// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include "SoapyRemoteConfig.hpp"
#include <string>

namespace SoapyInfo
{
    //! Get the hostname
    SOAPY_REMOTE_API std::string getHostName(void);

    /*!
     * Get a unique identification string for this process.
     * This is usually the combination of a locally-unique
     * process ID and a globally unique host/network ID.
     */
    SOAPY_REMOTE_API std::string uniqueProcessId(void);

    /*!
     * Get the user agent string for this build.
     */
    SOAPY_REMOTE_API std::string getUserAgent(void);
};
