// Copyright (c) 2015-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

/*
 * Docs and examples:
 * https://stackoverflow.com/questions/13382469/ssdp-protocol-implementation
 * http://buildingskb.schneider-electric.com/view.php?AID=15197
 * http://upnp.org/specs/arch/UPnP-arch-DeviceArchitecture-v1.1.pdf
 */

#include <SoapySDR/Logger.hpp>
#include "SoapySSDPEndpoint.hpp"
#include "SoapyURLUtils.hpp"
#include "SoapyInfoUtils.hpp"
#include "SoapyRemoteDefs.hpp"
#include "SoapyHTTPUtils.hpp"
#include "SoapyRPCSocket.hpp"
#include <thread>
#include <ctime>
#include <chrono>
#include <cctype>
#include <map>
#include <set>

//! IPv4 multi-cast address for SSDP communications
#define SSDP_MULTICAST_ADDR_IPV4 "239.255.255.250"

//! IPv6 multi-cast address for SSDP communications
#define SSDP_MULTICAST_ADDR_IPV6 "ff02::c"

//! UDP service port number for SSDP communications
#define SSDP_UDP_PORT_NUMBER "1900"

//! service and notify target identification string
#define SOAPY_REMOTE_TARGET "urn:schemas-pothosware-com:service:soapyRemote:1"

//! How often search and notify packets are triggered
#define TRIGGER_TIMEOUT_SECONDS 60

//! The default duration of an entry in the USN cache
#define CACHE_DURATION_SECONDS 120

//! Service is active, use with multicast NOTIFY
#define NTS_ALIVE "ssdp:alive"

//! Service stopped, use with multicast NOTIFY
#define NTS_BYEBYE "ssdp:byebye"

static std::string uuidFromUSN(const std::string &usn)
{
    auto posUUID = usn.find("uuid:");
    if (posUUID == std::string::npos) return usn;
    posUUID += 5;
    const auto posColon = usn.find(":", posUUID);
    if (posColon == std::string::npos) return usn;
    return usn.substr(posUUID, posColon-posUUID);
}

struct SoapySSDPEndpointData
{
    int ipVer;
    SoapyRPCSocket sock;
    std::string groupURL;
    std::thread *thread;
    std::chrono::high_resolution_clock::time_point lastTimeSearch;
    std::chrono::high_resolution_clock::time_point lastTimeNotify;
    typedef std::map<std::string, std::pair<std::string, std::chrono::high_resolution_clock::time_point>> DiscoveredURLs;
    DiscoveredURLs usnToURL;
};

static std::string timeNowGMT(void)
{
    char buff[128];
    auto t = std::time(nullptr);
    size_t len = std::strftime(buff, sizeof(buff), "%c %Z", std::localtime(&t));
    return std::string(buff, len);
}

SoapySSDPEndpoint::SoapySSDPEndpoint(void):
    serviceIpVer(SOAPY_REMOTE_IPVER_NONE),
    periodicSearchEnabled(false),
    periodicNotifyEnabled(false),
    done(false)
{
    const bool isIPv6Supported = not SoapyRPCSocket(SoapyURL("tcp", "::", "0").toString()).null();
    this->spawnHandler("0.0.0.0", SSDP_MULTICAST_ADDR_IPV4, 4);
    if (isIPv6Supported) this->spawnHandler("::", SSDP_MULTICAST_ADDR_IPV6, 6);
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

void SoapySSDPEndpoint::registerService(const std::string &uuid, const std::string &service, const int ipVer)
{
    std::lock_guard<std::mutex> lock(mutex);
    this->serviceIpVer = ipVer;
    this->uuid = uuid;
    this->service = service;
    periodicNotifyEnabled = true;
    for (auto &data : handlers) this->sendNotifyHeader(data, NTS_ALIVE);
}

std::map<std::string, std::map<int, std::string>> SoapySSDPEndpoint::getServerURLs(const int ipVer, const long timeoutUs)
{
    std::unique_lock<std::mutex> lock(mutex);

    //only needed if this is the first invocation...
    if (not periodicSearchEnabled)
    {
        periodicSearchEnabled = true;
        for (auto &data : handlers) this->sendSearchHeader(data);

        //wait maximum timeout for replies
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::microseconds(timeoutUs));
        lock.lock();
    }

    std::map<std::string, std::map<int, std::string>> serverUrls;

    for (auto &data : handlers)
    {
        if ((data->ipVer & ipVer) == 0) continue;
        for (auto &pair : data->usnToURL)
        {
            const auto uuid = uuidFromUSN(pair.first);
            serverUrls[uuid][data->ipVer] = pair.second.first;
        }
    }

    return serverUrls;
}

void SoapySSDPEndpoint::spawnHandler(const std::string &bindAddr, const std::string &groupAddr, const int ipVer)
{
    //static list of blacklisted groups
    //if we fail to join a group, its blacklisted
    //so future instances wont get the same error
    //thread-safe protected by the get instance call
    static std::set<std::string> blacklistedGroups;

    //check the blacklist
    if (blacklistedGroups.find(groupAddr) != blacklistedGroups.end())
    {
        SoapySDR::logf(SOAPY_SDR_DEBUG, "SoapySSDPEndpoint::spawnHandler(%s) group blacklisted due to previous error", groupAddr.c_str());
        return;
    }

    auto data = new SoapySSDPEndpointData();
    data->ipVer = ipVer;
    auto &sock = data->sock;

    const auto groupURL = SoapyURL("udp", groupAddr, SSDP_UDP_PORT_NUMBER).toString();
    int ret = sock.multicastJoin(groupURL);
    if (ret != 0)
    {
        blacklistedGroups.insert(groupAddr);
        SoapySDR::logf(SOAPY_SDR_WARNING, "SoapySSDPEndpoint failed join group %s\n  %s", groupURL.c_str(), sock.lastErrorMsg());
        delete data;
        return;
    }

    const auto bindURL = SoapyURL("udp", bindAddr, SSDP_UDP_PORT_NUMBER).toString();
    ret = sock.bind(bindURL);
    if (ret != 0)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "SoapySSDPEndpoint::bind(%s) failed\n  %s", bindURL.c_str(), sock.lastErrorMsg());
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
                SoapySDR::logf(SOAPY_SDR_ERROR, "SoapySSDPEndpoint::recvfrom() = %d\n  %s", ret, sock.lastErrorMsg());
                return;
            }

            //parse the HTTP header
            SoapyHTTPHeader header(recvBuff, size_t(ret));
            if (header.getLine0() == "M-SEARCH * HTTP/1.1") this->handleSearchRequest(data, header, recvAddr);
            if (header.getLine0() == "HTTP/1.1 200 OK") this->handleSearchResponse(data, header, recvAddr);
            if (header.getLine0() == "NOTIFY * HTTP/1.1") this->handleNotifyRequest(data, header, recvAddr);
        }

        //locked for all non-blocking routines below
        std::lock_guard<std::mutex> lock(mutex);
        const auto timeNow = std::chrono::high_resolution_clock::now();
        const auto triggerExpired = timeNow + std::chrono::seconds(TRIGGER_TIMEOUT_SECONDS);

        //remove old cache entries
        auto it = data->usnToURL.begin();
        while (it != data->usnToURL.end())
        {
            auto &expires = it->second.second;
            if (expires > timeNow) ++it;
            else data->usnToURL.erase(it++);
        }

        //check trigger for periodic search
        if (periodicSearchEnabled and data->lastTimeSearch > triggerExpired)
        {
            this->sendSearchHeader(data);
        }

        //check trigger for periodic notify
        if (periodicNotifyEnabled and data->lastTimeNotify > triggerExpired)
        {
            this->sendNotifyHeader(data, NTS_ALIVE);
        }
    }

    //disconnect notification when done
    if (done)
    {
        std::lock_guard<std::mutex> lock(mutex);
        this->sendNotifyHeader(data, NTS_BYEBYE);
    }
}

void SoapySSDPEndpoint::sendHeader(SoapyRPCSocket &sock, const SoapyHTTPHeader &header, const std::string &addr)
{
    int ret = sock.sendto(header.data(), header.size(), addr);
    if (ret != int(header.size()))
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "SoapySSDPEndpoint::sendTo(%s) = %d\n  %s", addr.c_str(), ret, sock.lastErrorMsg());
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

void SoapySSDPEndpoint::sendNotifyHeader(SoapySSDPEndpointData *data, const std::string &nts)
{
    if ((serviceIpVer & data->ipVer) == 0) return; //do we have a service to advertise?

    auto hostURL = SoapyURL(data->groupURL);
    hostURL.setScheme(""); //no scheme name

    SoapyHTTPHeader header("NOTIFY * HTTP/1.1");
    header.addField("HOST", hostURL.toString());
    if (nts == NTS_ALIVE)
    {
        header.addField("CACHE-CONTROL", "max-age=" + std::to_string(CACHE_DURATION_SECONDS));
        header.addField("LOCATION", SoapyURL("tcp", SoapyInfo::getHostName(), service).toString());
    }
    header.addField("SERVER", SoapyInfo::getUserAgent());
    header.addField("NT", SOAPY_REMOTE_TARGET);
    header.addField("USN", "uuid:"+uuid+"::"+SOAPY_REMOTE_TARGET);
    header.addField("NTS", nts);
    header.finalize();
    this->sendHeader(data->sock, header, data->groupURL);
    data->lastTimeNotify = std::chrono::high_resolution_clock::now();
}

void SoapySSDPEndpoint::handleSearchRequest(SoapySSDPEndpointData *data, const SoapyHTTPHeader &request, const std::string &recvAddr)
{
    if ((serviceIpVer & data->ipVer) == 0) return; //do we have a service to advertise?

    if (request.getField("MAN") != "\"ssdp:discover\"") return;
    const auto st = request.getField("ST");
    const bool stForUs = (st == "ssdp:all" or st == SOAPY_REMOTE_TARGET or st == "uuid:"+uuid);
    if (not stForUs) return;

    //send a unicast response HTTP header
    SoapyHTTPHeader response("HTTP/1.1 200 OK");
    response.addField("CACHE-CONTROL", "max-age=" + std::to_string(CACHE_DURATION_SECONDS));
    response.addField("DATE", timeNowGMT());
    response.addField("EXT", "");
    response.addField("LOCATION", SoapyURL("tcp", SoapyInfo::getHostName(), service).toString());
    response.addField("SERVER", SoapyInfo::getUserAgent());
    response.addField("ST", SOAPY_REMOTE_TARGET);
    response.addField("USN", "uuid:"+uuid+"::"+SOAPY_REMOTE_TARGET);
    response.finalize();
    this->sendHeader(data->sock, response, recvAddr);

    //The unicast response may not be received if the destination has multiple SSDP clients
    //because only one client on the destination host will actually receive the datagram.
    //To work around this limitation, a multicast notification packet is sent as well;
    //which will be received by all clients at the destination as well as other hosts.
    this->sendNotifyHeader(data, NTS_ALIVE);
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
    while (std::isspace(cacheControl.at(valuePos))) valuePos++;

    const auto maxAge = cacheControl.substr(valuePos);
    try {return std::stoul(maxAge);}
    catch (...) {return CACHE_DURATION_SECONDS;}
}

void SoapySSDPEndpoint::handleSearchResponse(SoapySSDPEndpointData *data, const SoapyHTTPHeader &header, const std::string &recvAddr)
{
    if (header.getField("ST") != SOAPY_REMOTE_TARGET) return;
    this->handleRegisterService(data, header, recvAddr);
}

void SoapySSDPEndpoint::handleNotifyRequest(SoapySSDPEndpointData *data, const SoapyHTTPHeader &header, const std::string &recvAddr)
{
    if (header.getField("NT") != SOAPY_REMOTE_TARGET) return;
    this->handleRegisterService(data, header, recvAddr);
}

void SoapySSDPEndpoint::handleRegisterService(SoapySSDPEndpointData *data, const SoapyHTTPHeader &header, const std::string &recvAddr)
{
    //extract usn
    const auto usn = header.getField("USN");
    if (usn.empty()) return;

    //handle byebye from notification packets
    if (header.getField("NTS") == NTS_BYEBYE)
    {
        SoapySDR::logf(SOAPY_SDR_DEBUG, "SoapySSDP removed %s [%s] IPv%d", data->usnToURL[usn].first.c_str(), uuidFromUSN(usn).c_str(), data->ipVer);
        data->usnToURL.erase(usn);
        return;
    }

    //format the server's url
    const auto location = header.getField("LOCATION");
    if (location.empty()) return;
    const SoapyURL serverURL("tcp", SoapyURL(recvAddr).getNode(), SoapyURL(location).getService());
    SoapySDR::logf(SOAPY_SDR_DEBUG, "SoapySSDP discovered %s [%s] IPv%d", serverURL.toString().c_str(), uuidFromUSN(usn).c_str(), data->ipVer);

    //register the server
    const auto expires = std::chrono::high_resolution_clock::now() + std::chrono::seconds(getCacheDuration(header));
    data->usnToURL[usn] = std::make_pair(serverURL.toString(), expires);
}
