// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapySSDPUtils.hpp"
#include "SoapyInfoUtils.hpp"
#include <ctime>

std::string formatMSearchRequest(
    const std::string &host,
    const int mx,
    const std::string &st,
    const std::string &man
){
    std::string out;
    out += "M-SEARCH * HTTP/1.1\r\n";
    out += "HOST: "+host+"\r\n";
    out += "MAN: \""+man+"\"\r\n";
    out += "MX: "+std::to_string(mx)+"\r\n";
    out += "ST: "+st+"\r\n";
    out += "USER-AGENT: "+SoapyInfo::getUserAgent()+"\r\n";
    out += "\r\n";
    return out;
}

std::string formatMSearchResponse(
    const std::string &location,
    const int maxAge,
    const std::string &st
){
    char buff[128];
    auto t = std::time(nullptr);
    size_t len = std::strftime(buff, sizeof(buff), "%c %Z", std::localtime(&t));
    std::string dateStr(buff, len);

    std::string out;
    out += "HTTP/1.1 200 OK\r\n";
    out += "CACHE-CONTROL: max-age="+std::to_string(maxAge)+"\r\n";
    out += "DATE: "+dateStr+"\r\n";
    out += "EXT: \r\n";
    out += "LOCATION: "+location+"\r\n";
    out += "SERVER: "+SoapyInfo::getUserAgent()+"\r\n";
    out += "ST: "+st+"\r\n";
    out += "USN: "+SoapyInfo::uniqueProcessId()+"\r\n";
    out += "\r\n";
    return out;
}
