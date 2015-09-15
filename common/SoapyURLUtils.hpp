// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include "SoapyRemoteConfig.hpp"
#include <cstddef>
#include <string>

struct sockaddr;

/*!
 * Utility to parse and lookup a URL string.
 */
SOAPY_REMOTE_API bool lookupURL(const std::string &url,
    int &af, int &type, int &prot,
    struct sockaddr &addr, int &addrlen,
    std::string &errorMsg);

/*!
 * Convert a socket structure into a URL string.
 */
SOAPY_REMOTE_API std::string sockaddrToURL(const struct sockaddr &addr);

/*!
 * Get a unique identification string for this process.
 * This is usually the combination of a locally-unique
 * process ID and a globally unique host/network ID.
 */
SOAPY_REMOTE_API std::string uniqueProcessId(void);
