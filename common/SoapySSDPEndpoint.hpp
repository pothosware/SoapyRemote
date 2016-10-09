// Copyright (c) 2015-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include "SoapyRPCSocket.hpp"
#include <string>
#include <csignal> //sig_atomic_t
#include <mutex>
#include <vector>
#include <memory>

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

    //! Get a singleton instance of the endpoint
    static std::shared_ptr<SoapySSDPEndpoint> getInstance(void);

    /*!
     * Create a discovery endpoint
     */
    SoapySSDPEndpoint(void);

    ~SoapySSDPEndpoint(void);

    /*!
     * Allow the endpoint to advertise that its running the RPC service
     */
    void registerService(const std::string &uuid, const std::string &service);

    /*!
     * Enable the client endpoint to search for running services.
     */
    void enablePeriodicSearch(const bool enable);

    /*!
     * Enable the server to send periodic notification messages.
     */
    void enablePeriodicNotify(const bool enable);

    /*!
     * Get a list of all active server URLs.
     *
     * The same endpoint can be discovered under both IPv4 and IPv6.
     * When 'only' is false, the ipVer specifies the IP version preference
     * when both are discovered but will fallback to the other version.
     * But when 'only' is true, only addresses of the ipVer type are used.
     *
     * \param ipVer the preferred IP version to discover (6 or 4)
     * \param only true to ignore other discovered IP versions
     */
    std::vector<std::string> getServerURLs(const int ipVer = 4, const bool only = false);

private:
    SoapySocketSession sess;

    //protection between threads
    std::mutex mutex;

    //service settings
    bool serviceRegistered;
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
