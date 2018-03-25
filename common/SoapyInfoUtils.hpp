// Copyright (c) 2015-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include "SoapyRemoteConfig.hpp"
#include <string>

namespace SoapyInfo
{
    //! Get the hostname
    SOAPY_REMOTE_API std::string getHostName(void);

    /*!
     * Generate a type1 UUID based on the current time and host ID.
     */
    SOAPY_REMOTE_API std::string generateUUID1(void);

    /*!
     * Get the user agent string for this build.
     */
    SOAPY_REMOTE_API std::string getUserAgent(void);

    /*!
     * Get the server version string for this build.
     */
    SOAPY_REMOTE_API std::string getServerVersion(void);
};
