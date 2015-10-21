// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapySSDPEndpoint.hpp"
#include "SoapyURLUtils.hpp"
#include "SoapyInfoUtils.hpp"
#include "SoapyRemoteDefs.hpp"
#include "SoapyHTTPUtils.hpp"
#include "SoapyRPCSocket.hpp"
#include <thread>
#include <iostream>
#include <ctime>

static std::string timeNowGMT(void)
{
    char buff[128];
    auto t = std::time(nullptr);
    size_t len = std::strftime(buff, sizeof(buff), "%c %Z", std::localtime(&t));
    return std::string(buff, len);
}

SoapySSDPEndpoint::SoapySSDPEndpoint(const int ipVersion):
    workerThread(nullptr),
    done(false)
{
    groupURL = SoapyURL("udp", (ipVersion==4)?"239.255.255.250":"ff02::c", "1900").toString();
    int ret = sock.multicastJoin(groupURL);
    if (ret != 0)
    {
        std::cerr << "SoapySSDPEndpoint::multicastJoin(" << groupURL << ") failed: " << sock.lastErrorMsg() << std::endl;
        return;
    }

    const auto bindURL = SoapyURL("udp", (ipVersion==4)?"0.0.0.0":"::", "1900").toString();
    ret = sock.bind(bindURL);
    if (ret != 0)
    {
        std::cerr << "SoapySSDPEndpoint::bind(" << bindURL << ") failed: " << sock.lastErrorMsg() << std::endl;
        return;
    }

    workerThread = new std::thread(&SoapySSDPEndpoint::handlerLoop, this);
}

SoapySSDPEndpoint::~SoapySSDPEndpoint(void)
{
    done = true;
    if (workerThread != nullptr) workerThread->join();
    delete workerThread;
}

void SoapySSDPEndpoint::enablePeriodicSearch(void)
{
    auto hostURL = SoapyURL(groupURL);
    hostURL.setScheme(""); //no scheme name

    //TODO actually enable
    SoapyHTTPHeader header("M-SEARCH * HTTP/1.1");
    header.addField("HOST", hostURL.toString());
    header.addField("MAN", "\"ssdp:discover\"");
    header.addField("MX", "2");
    header.addField("ST", SOAPY_REMOTE_SEARCH_TARGET);
    header.addField("USER-AGENT", SoapyInfo::getUserAgent());

    int ret = sock.sendto(header.data(), header.size(), groupURL);
}

void SoapySSDPEndpoint::handlerLoop(void)
{
    std::string recvAddr;
    char recvBuff[SOAPY_REMOTE_DEFAULT_ENDPOINT_MTU];

    while (not done)
    {
        //receive SSDP traffic
        if (not sock.selectRecv(SOAPY_REMOTE_SOCKET_TIMEOUT_US)) continue;
        int ret = sock.recvfrom(recvBuff, sizeof(recvBuff), recvAddr);
        if (ret < 0)
        {
            std::cerr << "SoapySSDPEndpoint::recvfrom() failed: ret=" << ret << ", " << sock.lastErrorMsg() << std::endl;
            return;
        }

        //parse the HTTP header
        SoapyHTTPHeader recvHdr(recvBuff, size_t(ret));
        if (service.empty()) continue;
        if (recvHdr.getLine0() != "M-SEARCH * HTTP/1.1") continue;
        if (recvHdr.getField("MAN") != "\"ssdp:discover\"") continue;
        const auto st = recvHdr.getField("ST");
        const bool stForUs = (st == "ssdp:all" or st == SOAPY_REMOTE_SEARCH_TARGET or st == "uuid:"+SoapyInfo::uniqueProcessId());
        if (not stForUs) continue;

        //send a response HTTP header
        auto location = SoapyURL("tcp", SoapyInfo::getHostName(), service).toString();

        SoapyHTTPHeader sendHdr("HTTP/1.1 200 OK");
        sendHdr.addField("CACHE-CONTROL", "max-age=180");
        sendHdr.addField("DATE", timeNowGMT());
        sendHdr.addField("EXT", "");
        sendHdr.addField("LOCATION", location);
        sendHdr.addField("SERVER", SoapyInfo::getUserAgent());
        sendHdr.addField("ST", st);
        sendHdr.addField("USN", "uuid:"+SoapyInfo::uniqueProcessId()+"::"+st);

        ret = sock.sendto(sendHdr.data(), sendHdr.size(), recvAddr);
        std::cout << "sent reply\n";
        if (ret != int(sendHdr.size()))
        {
            std::cerr << "SoapySSDPEndpoint::sendTo(" << recvAddr << ") failed: ret=" << ret << ", " << sock.lastErrorMsg() << std::endl;
            return;
        }
    }
}
