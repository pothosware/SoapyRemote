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
#include <cctype>

//! service and notify target identification string
#define SOAPY_REMOTE_TARGET "urn:schemas-pothosware-com:service:soapyRemote:1"

//! How often search and notify packets are triggered
#define TRIGGER_TIMEOUT_SECONDS 60

//! The duration of an entry in the USN cache
#define CACHE_DURATION_SECONDS 120

struct SoapySSDPEndpointData
{
    SoapyRPCSocket sock;
    std::string groupURL;
    std::thread *thread;
    std::chrono::high_resolution_clock::time_point lastTimeSearch;
    std::chrono::high_resolution_clock::time_point lastTimeNotify;
};

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
    serviceRegistered(false),
    periodicSearchEnabled(false),
    periodicNotifyEnabled(false),
    done(false)
{
    const bool isIPv6Supported = not SoapyRPCSocket(SoapyURL("tcp", "::", "0").toString()).null();
    this->spawnHandler("0.0.0.0", "239.255.255.250");
    if (isIPv6Supported) this->spawnHandler("::", "ff02::c");
}

SoapySSDPEndpoint::~SoapySSDPEndpoint(void)
{
    done = true;
    for (auto &data : handlers)
    {
        data->thread->join();
        delete data->thread;
        delete data;
    }
}

void SoapySSDPEndpoint::registerService(const std::string &uuid, const std::string &service)
{
    std::lock_guard<std::mutex> lock(mutex);
    this->serviceRegistered = true;
    this->uuid = uuid;
    this->service = service;
}

void SoapySSDPEndpoint::enablePeriodicSearch(const bool enable)
{
    std::lock_guard<std::mutex> lock(mutex);
    periodicSearchEnabled = enable;
    for (auto &data : handlers) this->sendSearchHeader(data);
}

void SoapySSDPEndpoint::enablePeriodicNotify(const bool enable)
{
    std::lock_guard<std::mutex> lock(mutex);
    periodicNotifyEnabled = enable;
    for (auto &data : handlers) this->sendNotifyHeader(data, true);
}

std::vector<std::string> SoapySSDPEndpoint::getServerURLs(void)
{
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<std::string> serverURLs;
    for (auto &pair : usnToURL) serverURLs.push_back(pair.second.first);
    return serverURLs;
}

void SoapySSDPEndpoint::spawnHandler(const std::string &bindAddr, const std::string &groupAddr)
{
    auto data = new SoapySSDPEndpointData();
    auto &sock = data->sock;

    const auto groupURL = SoapyURL("udp", groupAddr, "1900").toString();
    int ret = sock.multicastJoin(groupURL);
    if (ret != 0)
    {
        std::cerr << "SoapySSDPEndpoint::multicastJoin(" << groupURL << ") failed: " << sock.lastErrorMsg() << std::endl;
        delete data;
        return;
    }

    const auto bindURL = SoapyURL("udp", bindAddr, "1900").toString();
    ret = sock.bind(bindURL);
    if (ret != 0)
    {
        std::cerr << "SoapySSDPEndpoint::bind(" << bindURL << ") failed: " << sock.lastErrorMsg() << std::endl;
        delete data;
        return;
    }

    data->groupURL = groupURL;
    data->thread = new std::thread(&SoapySSDPEndpoint::handlerLoop, this, data);
    handlers.push_back(data);
}

void SoapySSDPEndpoint::handlerLoop(SoapySSDPEndpointData *data)
{
    auto &sock = data->sock;

    std::string recvAddr;
    char recvBuff[SOAPY_REMOTE_DEFAULT_ENDPOINT_MTU];

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
        const auto triggerExpired = timeNow + std::chrono::seconds(TRIGGER_TIMEOUT_SECONDS);

        //remove old cache entries
        auto it = usnToURL.begin();
        while (it != usnToURL.end())
        {
            auto &expires = it->second.second;
            if (expires > timeNow) ++it;
            else usnToURL.erase(it++);
        }

        //check trigger for periodic search
        if (periodicSearchEnabled and data->lastTimeSearch > triggerExpired)
        {
            this->sendSearchHeader(data);
        }

        //check trigger for periodic notify
        if (periodicNotifyEnabled and data->lastTimeNotify > triggerExpired)
        {
            this->sendNotifyHeader(data, true);
        }
    }

    //disconnect notification when done
    if (done)
    {
        this->sendNotifyHeader(data, false);
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

void SoapySSDPEndpoint::sendSearchHeader(SoapySSDPEndpointData *data)
{
    auto hostURL = SoapyURL(data->groupURL);
    hostURL.setScheme(""); //no scheme name

    SoapyHTTPHeader header("M-SEARCH * HTTP/1.1");
    header.addField("HOST", hostURL.toString());
    header.addField("MAN", "\"ssdp:discover\"");
    header.addField("MX", "2");
    header.addField("ST", SOAPY_REMOTE_TARGET);
    header.addField("USER-AGENT", SoapyInfo::getUserAgent());
    header.finalize();
    this->sendHeader(data->sock, header, data->groupURL);
    data->lastTimeSearch = std::chrono::high_resolution_clock::now();
}

void SoapySSDPEndpoint::sendNotifyHeader(SoapySSDPEndpointData *data, const bool alive)
{
    if (not serviceRegistered) return; //do we have a service to advertise?

    auto hostURL = SoapyURL(data->groupURL);
    hostURL.setScheme(""); //no scheme name

    SoapyHTTPHeader header("NOTIFY * HTTP/1.1");
    header.addField("HOST", hostURL.toString());
    if (alive) header.addField("CACHE-CONTROL", "max-age=" + std::to_string(CACHE_DURATION_SECONDS));
    if (alive) header.addField("LOCATION", SoapyURL("tcp", SoapyInfo::getHostName(), service).toString());
    header.addField("SERVER", SoapyInfo::getUserAgent());
    header.addField("NT", SOAPY_REMOTE_TARGET);
    header.addField("USN", "uuid:"+uuid+"::"+SOAPY_REMOTE_TARGET);
    header.addField("NTS", alive?"ssdp:alive":"ssdp:byebye");
    header.finalize();
    this->sendHeader(data->sock, header, data->groupURL);
    data->lastTimeNotify = std::chrono::high_resolution_clock::now();
}

void SoapySSDPEndpoint::handleSearchRequest(SoapyRPCSocket &sock, const SoapyHTTPHeader &request, const std::string &recvAddr)
{
    if (not serviceRegistered) return; //do we have a service to advertise?

    if (request.getField("MAN") != "\"ssdp:discover\"") return;
    const auto st = request.getField("ST");
    const bool stForUs = (st == "ssdp:all" or st == SOAPY_REMOTE_TARGET or st == "uuid:"+uuid);
    if (not stForUs) return;

    //send a response HTTP header
    SoapyHTTPHeader response("HTTP/1.1 200 OK");
    response.addField("CACHE-CONTROL", "max-age=" + std::to_string(CACHE_DURATION_SECONDS));
    response.addField("DATE", timeNowGMT());
    response.addField("EXT", "");
    response.addField("LOCATION", SoapyURL("tcp", SoapyInfo::getHostName(), service).toString());
    response.addField("SERVER", SoapyInfo::getUserAgent());
    response.addField("ST", SOAPY_REMOTE_TARGET);
    response.addField("USN", "uuid:"+uuid+"::"+SOAPY_REMOTE_TARGET);
    response.finalize();
    this->sendHeader(sock, response, recvAddr);
}

static int getCacheDuration(const SoapyHTTPHeader &header)
{
    const auto cacheControl = header.getField("CACHE-CONTROL");
    if (cacheControl.empty()) return CACHE_DURATION_SECONDS;

    const auto maxAgePos = cacheControl.find("max-age");
    const auto equalsPos = cacheControl.find("=");
    if (maxAgePos == std::string::npos) return CACHE_DURATION_SECONDS;
    if (equalsPos == std::string::npos) return CACHE_DURATION_SECONDS;
    if (maxAgePos > equalsPos) return CACHE_DURATION_SECONDS;
    auto valuePos = equalsPos + 1;
    while (std::isblank(cacheControl.at(valuePos))) valuePos++;

    const auto maxAge = cacheControl.substr(valuePos);
    try {return std::stoul(maxAge);}
    catch (...) {return CACHE_DURATION_SECONDS;}
}

void SoapySSDPEndpoint::handleSearchResponse(SoapyRPCSocket &, const SoapyHTTPHeader &header, const std::string &recvAddr)
{
    if (header.getField("ST") != SOAPY_REMOTE_TARGET) return;
    const SoapyURL locationUrl(header.getField("LOCATION"));
    const SoapyURL serverURL("tcp", SoapyURL(recvAddr).getNode(), locationUrl.getService());

    const auto expires = std::chrono::high_resolution_clock::now() + std::chrono::seconds(getCacheDuration(header));
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
        const auto expires = std::chrono::high_resolution_clock::now() + std::chrono::seconds(getCacheDuration(header));
        usnToURL[header.getField("USN")] = std::make_pair(serverURL.toString(), expires);
    }
}
