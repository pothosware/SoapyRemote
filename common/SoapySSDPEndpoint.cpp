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

//! service and notify target identification string
#define SOAPY_REMOTE_TARGET "urn:schemas-pothosware-com:service:soapyRemote:1"

static std::string timeNowGMT(void)
{
    char buff[128];
    auto t = std::time(nullptr);
    size_t len = std::strftime(buff, sizeof(buff), "%c %Z", std::localtime(&t));
    return std::string(buff, len);
}

SoapySSDPEndpoint *SoapySSDPEndpoint::getInstance(void)
{
    static SoapySSDPEndpoint ep;
    return &ep;
}

SoapySSDPEndpoint::SoapySSDPEndpoint(void):
    uuid(SoapyInfo::uniqueProcessId()),
    periodicSearchEnabled(false),
    periodicNotifyEnabled(false),
    workerThreadv4(nullptr),
    workerThreadv6(nullptr),
    done(false)
{
    const bool isIPv6Supported = not SoapyRPCSocket(SoapyURL("tcp", "::", "0").toString()).null();
    workerThreadv4 = new std::thread(&SoapySSDPEndpoint::handlerLoop, this, 4);
    if (isIPv6Supported) workerThreadv6 = new std::thread(&SoapySSDPEndpoint::handlerLoop, this, 6);
}

SoapySSDPEndpoint::~SoapySSDPEndpoint(void)
{
    done = true;
    if (workerThreadv4 != nullptr) workerThreadv4->join();
    if (workerThreadv6 != nullptr) workerThreadv6->join();
    delete workerThreadv4;
    delete workerThreadv6;
}

void SoapySSDPEndpoint::advertiseService(const std::string &service)
{
    std::lock_guard<std::mutex> lock(mutex);
    this->service = service;
}

void SoapySSDPEndpoint::enablePeriodicSearch(const bool enable)
{
    std::lock_guard<std::mutex> lock(mutex);
    periodicSearchEnabled = enable;
}

void SoapySSDPEndpoint::enablePeriodicNotify(const bool enable)
{
    std::lock_guard<std::mutex> lock(mutex);
    periodicNotifyEnabled = enable;
}

std::vector<std::string> SoapySSDPEndpoint::getServerURLs(void)
{
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<std::string> serverURLs;
    for (auto &pair : usnToURL) serverURLs.push_back(pair.second.first);
    return serverURLs;
}

void SoapySSDPEndpoint::handlerLoop(const int ipVersion)
{
    SoapySocketSession sess;
    SoapyRPCSocket sock;

    const auto groupURL = SoapyURL("udp", (ipVersion==4)?"239.255.255.250":"ff02::c", "1900").toString();
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

    auto hostURL = SoapyURL(groupURL);
    hostURL.setScheme(""); //no scheme name

    std::string recvAddr;
    char recvBuff[SOAPY_REMOTE_DEFAULT_ENDPOINT_MTU];

    std::chrono::high_resolution_clock::time_point nextSearchTrigger;
    std::chrono::high_resolution_clock::time_point nextNotifyTrigger;

    while (not done)
    {
        //receive SSDP traffic
        if (sock.selectRecv(SOAPY_REMOTE_SOCKET_TIMEOUT_US))
        {
            std::lock_guard<std::mutex> lock(mutex);
            int ret = sock.recvfrom(recvBuff, sizeof(recvBuff), recvAddr);
            if (ret < 0)
            {
                std::cerr << "SoapySSDPEndpoint::recvfrom() failed: ret=" << ret << ", " << sock.lastErrorMsg() << std::endl;
                return;
            }

            //parse the HTTP header
            SoapyHTTPHeader header(recvBuff, size_t(ret));
            if (header.getLine0() == "M-SEARCH * HTTP/1.1") this->handleSearchRequest(sock, header, recvAddr);
            if (header.getLine0() == "HTTP/1.1 200 OK") this->handleSearchResponse(sock, header, recvAddr);
            if (header.getLine0() == "NOTIFY * HTTP/1.1") this->handleNotifyRequest(sock, header, recvAddr);
        }

        //locked for all non-blocking routines below
        std::lock_guard<std::mutex> lock(mutex);
        const auto timeNow = std::chrono::high_resolution_clock::now();

        //remove old cache entries
        auto it = usnToURL.begin();
        while (it != usnToURL.end())
        {
            auto &expires = it->second.second;
            if (expires > timeNow) ++it;
            else usnToURL.erase(it++);
        }

        //check trigger for periodic search
        if (periodicSearchEnabled and nextSearchTrigger <= timeNow)
        {
            SoapyHTTPHeader header("M-SEARCH * HTTP/1.1");
            header.addField("HOST", hostURL.toString());
            header.addField("MAN", "\"ssdp:discover\"");
            header.addField("MX", "2");
            header.addField("ST", SOAPY_REMOTE_TARGET);
            header.addField("USER-AGENT", SoapyInfo::getUserAgent());
            header.finalize();
            this->sendHeader(sock, header, groupURL);

            nextSearchTrigger = timeNow + std::chrono::seconds(120);
        }

        //check trigger for periodic notify
        if (periodicNotifyEnabled and nextNotifyTrigger <= timeNow)
        {
            SoapyHTTPHeader header("NOTIFY * HTTP/1.1");
            header.addField("HOST", hostURL.toString());
            header.addField("CACHE-CONTROL", "max-age=120");
            header.addField("LOCATION", SoapyURL("tcp", SoapyInfo::getHostName(), service).toString());
            header.addField("SERVER", SoapyInfo::getUserAgent());
            header.addField("NT", SOAPY_REMOTE_TARGET);
            header.addField("USN", "uuid:"+uuid+"::"+SOAPY_REMOTE_TARGET);
            header.addField("NTS", "ssdp:alive");
            header.finalize();
            this->sendHeader(sock, header, groupURL);

            nextNotifyTrigger = timeNow + std::chrono::seconds(120);
        }
    }

    //send the byebye notification when done
    if (done)
    {
        SoapyHTTPHeader header("NOTIFY * HTTP/1.1");
        header.addField("HOST", hostURL.toString());
        header.addField("SERVER", SoapyInfo::getUserAgent());
        header.addField("NT", SOAPY_REMOTE_TARGET);
        header.addField("USN", "uuid:"+uuid+"::"+SOAPY_REMOTE_TARGET);
        header.addField("NTS", "ssdp:byebye");
        header.finalize();
        this->sendHeader(sock, header, groupURL);
    }
}

void SoapySSDPEndpoint::sendHeader(SoapyRPCSocket &sock, const SoapyHTTPHeader &header, const std::string &addr)
{
    int ret = sock.sendto(header.data(), header.size(), addr);
    if (ret != int(header.size()))
    {
        std::cerr << "SoapySSDPEndpoint::sendTo(" << addr << ") failed: ret=" << ret << ", " << sock.lastErrorMsg() << std::endl;
    }
}

void SoapySSDPEndpoint::handleSearchRequest(SoapyRPCSocket &sock, const SoapyHTTPHeader &request, const std::string &recvAddr)
{
    if (service.empty()) return; //do we have a service to advertise?

    if (request.getField("MAN") != "\"ssdp:discover\"") return;
    const auto st = request.getField("ST");
    const bool stForUs = (st == "ssdp:all" or st == SOAPY_REMOTE_TARGET or st == "uuid:"+uuid);
    if (not stForUs) return;

    //send a response HTTP header
    SoapyHTTPHeader response("HTTP/1.1 200 OK");
    response.addField("CACHE-CONTROL", "max-age=120");
    response.addField("DATE", timeNowGMT());
    response.addField("EXT", "");
    response.addField("LOCATION", SoapyURL("tcp", SoapyInfo::getHostName(), service).toString());
    response.addField("SERVER", SoapyInfo::getUserAgent());
    response.addField("ST", st);
    response.addField("USN", "uuid:"+uuid+"::"+st);
    response.finalize();
    this->sendHeader(sock, response, recvAddr);
}

void SoapySSDPEndpoint::handleSearchResponse(SoapyRPCSocket &, const SoapyHTTPHeader &header, const std::string &recvAddr)
{
    if (header.getField("ST") != SOAPY_REMOTE_TARGET) return;
    const SoapyURL locationUrl(header.getField("LOCATION"));
    const SoapyURL serverURL("tcp", SoapyURL(recvAddr).getNode(), locationUrl.getService());

    const auto expires = std::chrono::high_resolution_clock::now() + std::chrono::seconds(120);
    usnToURL[header.getField("USN")] = std::make_pair(serverURL.toString(), expires);
}

void SoapySSDPEndpoint::handleNotifyRequest(SoapyRPCSocket &, const SoapyHTTPHeader &header, const std::string &recvAddr)
{
    if (header.getField("NT") != SOAPY_REMOTE_TARGET) return;
    const SoapyURL locationUrl(header.getField("LOCATION"));
    const SoapyURL serverURL("tcp", SoapyURL(recvAddr).getNode(), locationUrl.getService());

    if (header.getField("NTS") == "ssdp:byebye")
    {
        usnToURL.erase(header.getField("USN"));
    }
    else
    {
        const auto expires = std::chrono::high_resolution_clock::now() + std::chrono::seconds(120);
        usnToURL[header.getField("USN")] = std::make_pair(serverURL.toString(), expires);
    }
}
