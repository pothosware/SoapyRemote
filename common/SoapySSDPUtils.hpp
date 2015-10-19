// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include "SoapyRemoteConfig.hpp"
#include <string>

SOAPY_REMOTE_API std::string formatMSearchRequest(
    const std::string &host,
    const int mx = 2,
    const std::string &st = "soapysdr:remote",
    const std::string &man = "ssdp:discover"
);

SOAPY_REMOTE_API std::string formatMSearchResponse(
    const std::string &location,
    const int maxAge = 180,
    const std::string &st = "soapysdr:remote"
);
