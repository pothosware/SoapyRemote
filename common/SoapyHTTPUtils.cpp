// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyHTTPUtils.hpp"
#include <cctype>

SoapyHTTPHeader::SoapyHTTPHeader(const std::string &line0)
{
    _storage = line0 + "\r\n";
}

void SoapyHTTPHeader::addField(const std::string &key, const std::string &value)
{
    _storage += key + ": " + value + "\r\n";
}

void SoapyHTTPHeader::finalize(void)
{
    _storage += "\r\n";
}

SoapyHTTPHeader::SoapyHTTPHeader(const void *buff, const size_t length)
{
    _storage = std::string((const char *)buff, length);
}

std::string SoapyHTTPHeader::getLine0(void) const
{
    const auto pos = _storage.find("\r\n");
    if (pos == std::string::npos) return "";
    return _storage.substr(0, pos);
}

std::string SoapyHTTPHeader::getField(const std::string &key) const
{
    //find the field start
    const std::string fieldStart("\r\n"+key+":");
    auto pos = _storage.find(fieldStart);
    if (pos == std::string::npos) return "";

    //offset from field start
    pos += fieldStart.length();

    //offset from whitespace
    while (std::isspace(_storage.at(pos))) pos++;

    //find the field end
    const auto end = _storage.find("\r\n", pos);
    if (end == std::string::npos) return "";

    return _storage.substr(pos, end-pos);
}
