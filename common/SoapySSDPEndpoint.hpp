// Copyright (c) 2015-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include "SoapyRPCSocket.hpp"
#include <string>
#include <csignal> //sig_atomic_t
#include <mutex>
#include <vector>
#include <map>

class SoapyHTTPHeader;
struct SoapySSDPEndpointData;

/*!
 * Service an SSDP endpoint to:
 * keep track of discovered servers of interest,
 * and to respond to discovery packets for us.
 */
class SoapySSDPEndpoint
{
public:

    /*!
     * Create a discovery endpoint
     */
    SoapySSDPEndpoint(void);

    ~SoapySSDPEndpoint(void);

    /*!
     * Allow the endpoint to advertise that its running the RPC service
     */
    void registerService(const std::string &uuid, const std::string &service, const int ipVer);

    /*!
     * Enable the client endpoint to search for running services.
     */
    void enablePeriodicSearch(const bool enable);

    //! Is the periodic search already enabled?
    bool isPeriodicSearchEnabled(void) const
    {
        return periodicSearchEnabled;
    }

    /*!
     * Enable the server to send periodic notification messages.
     */
    void enablePeriodicNotify(const bool enable);

    /*!
     * Get a list of all active server URLs.
     * \param ipVer the preferred IP version to discover
     * \return a mapping of server UUIDs to host URLs
     */
    std::map<std::string, std::map<int, std::string>> getServerURLs(const int ipVer);

private:
    SoapySocketSession sess;

    //protection between threads
    std::mutex mutex;

    //service settings
    int serviceIpVer;
    std::string uuid;
    std::string service;

    //configured messages
    bool periodicSearchEnabled;
    bool periodicNotifyEnabled;

    //server data
    std::vector<SoapySSDPEndpointData *> handlers;

    //signal done to the thread
    sig_atomic_t done;

    void spawnHandler(const std::string &bindAddr, const std::string &groupAddr, const int ipVer);
    void handlerLoop(SoapySSDPEndpointData *data);
    void sendHeader(SoapyRPCSocket &sock, const SoapyHTTPHeader &header, const std::string &addr);
    void sendSearchHeader(SoapySSDPEndpointData *data);
    void sendNotifyHeader(SoapySSDPEndpointData *data, const std::string &nts);
    void handleSearchRequest(SoapySSDPEndpointData *data, const SoapyHTTPHeader &header, const std::string &addr);
    void handleSearchResponse(SoapySSDPEndpointData *data, const SoapyHTTPHeader &header, const std::string &addr);
    void handleNotifyRequest(SoapySSDPEndpointData *data, const SoapyHTTPHeader &header, const std::string &addr);
    void handleRegisterService(SoapySSDPEndpointData *, const SoapyHTTPHeader &header, const std::string &recvAddr);
};
