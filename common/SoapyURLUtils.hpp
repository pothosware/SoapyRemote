// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include "SoapyRemoteCommon.hpp"
#include <cstddef>
#include <string>

struct sockaddr;

/*!
 * Utility to parse and lookup a URL string.
 */
bool lookupURL(const std::string &url,
    int &af, int &type, int &prot,
    struct sockaddr &addr, int &addrlen,
    std::string &errorMsg);

/*!
 * Convert a socket structure into a URL string.
 */
std::string sockaddrToURL(const struct sockaddr &addr);
