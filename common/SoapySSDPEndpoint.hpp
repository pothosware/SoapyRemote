// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include "SoapyRPCSocket.hpp"
#include <map>
#include <string>
#include <csignal> //sig_atomic_t
#include <chrono>
#include <mutex>
#include <vector>

namespace std
{
    class thread;
}

class SoapyRPCSocket;
class SoapyHTTPHeader;

/*!
 * Service an SSDP endpoint to:
 * keep track of discovered servers of interest,
 * and to respond to discovery packets for us.
 */
class SoapySSDPEndpoint
{
public:

    //! Get a singleton instance of the endpoint
    static SoapySSDPEndpoint *getInstance(void);

    /*!
     * Create a discovery endpoint
     */
    SoapySSDPEndpoint(void);

    ~SoapySSDPEndpoint(void);

    /*!
     * Allow the endpoint to advertise that its running the RPC service
     */
    void advertiseService(const std::string &service);

    /*!
     * Enable the client endpoint to search for running services.
     */
    void enablePeriodicSearch(const bool enable);

    /*!
     * Enable the server to send periodic notification messages.
     */
    void enablePeriodicNotify(const bool enable);

    //! Get a list of all active server URLs
    std::vector<std::string> getServerURLs(void);

private:
    //protection between threads
    std::mutex mutex;
    std::map<std::string, std::pair<std::string, std::chrono::high_resolution_clock::time_point>> usnToURL;

    //uuid for this instance
    const std::string uuid;

    //name of the service port to advertise
    std::string service;

    //configured messages
    bool periodicSearchEnabled;
    bool periodicNotifyEnabled;

    //server handler thread
    std::thread *workerThreadv4;
    std::thread *workerThreadv6;

    //signal done to the thread
    sig_atomic_t done;

    void handlerLoop(const int ipVersion);
    void sendHeader(SoapyRPCSocket &sock, const SoapyHTTPHeader &header, const std::string &addr);
    void handleSearchRequest(SoapyRPCSocket &sock, const SoapyHTTPHeader &header, const std::string &addr);
    void handleSearchResponse(SoapyRPCSocket &sock, const SoapyHTTPHeader &header, const std::string &addr);
    void handleNotifyRequest(SoapyRPCSocket &sock, const SoapyHTTPHeader &header, const std::string &addr);
};
