// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <SoapySDR/Config.hpp>

//! The default bind port for the remote server
#define SOAPY_REMOTE_DEFAULT_SERVICE "55132"

//! Use this key with device arguments to specify remote server url
#define SOAPY_REMOTE_KWARG_KEY "remote"

enum SoapyRemoteTypes
{
    SOAPY_REMOTE_CHAR            = 0,
    SOAPY_REMOTE_BOOL            = 1,
    SOAPY_REMOTE_INT32           = 2,
    SOAPY_REMOTE_INT64           = 3,
    SOAPY_REMOTE_FLOAT64         = 4,
    SOAPY_REMOTE_COMPLEX128      = 5,
    SOAPY_REMOTE_STRING          = 6,
    SOAPY_REMOTE_RANGE           = 7,
    SOAPY_REMOTE_RANGE_LIST      = 8,
    SOAPY_REMOTE_STRING_LIST     = 9,
    SOAPY_REMOTE_FLOAT64_LIST    = 10,
    SOAPY_REMOTE_KWARGS          = 11,
    SOAPY_REMOTE_KWARGS_LIST     = 12,
    SOAPY_REMOTE_TYPE_MAX        = 13,
};
