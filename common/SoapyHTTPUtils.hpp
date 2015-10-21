// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include "SoapyRemoteConfig.hpp"
#include <string>

class SOAPY_REMOTE_API SoapyHTTPHeader
{
public:

    //! Create an HTTP header given request/response line
    SoapyHTTPHeader(const std::string &line0);

    //! Add a key/value field to the header
    void addField(const std::string &key, const std::string &value);

    //! Done adding fields to the header
    void finalize(void);

    //! Create an HTTP from a received datagram
    SoapyHTTPHeader(const void *buff, const size_t length);

    //! Get the request/response line
    std::string getLine0(void) const;

    //! Read a field from the HTTP header (empty when missing)
    std::string getField(const std::string &key) const;

    const void *data(void) const
    {
        return _storage.data();
    }

    size_t size(void) const
    {
        return _storage.size();
    }

private:
    std::string _storage;
};
