// Copyright (c) 2015-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include "SoapyRemoteConfig.hpp"
#include <string>
#include <map>

class SoapyHTTPHeader;
class SoapyRPCSocket;
struct SoapySSDPEndpointImpl;
struct SoapySSDPEndpointData;

/*!
 * Service an SSDP endpoint to:
 * keep track of discovered servers of interest,
 * and to respond to discovery packets for us.
 */
class SOAPY_REMOTE_API SoapySSDPEndpoint
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
     * Get a list of all active server URLs.
     * \param ipVer the preferred IP version to discover
     * \return a mapping of server UUIDs to host URLs
     */
    std::map<std::string, std::map<int, std::string>> getServerURLs(const int ipVer, const long timeoutUs);

private:
    SoapySSDPEndpointImpl *_impl;

    //service settings
    int serviceIpVer;
    std::string uuid;
    std::string service;

    //configured messages
    bool periodicSearchEnabled;
    bool periodicNotifyEnabled;

    void handlerLoop(void);
    void sendHeader(SoapyRPCSocket &sock, const SoapyHTTPHeader &header, const std::string &addr);
    void sendSearchHeader(SoapySSDPEndpointData *data);
    void sendNotifyHeader(SoapySSDPEndpointData *data, const std::string &nts);
    void handleSearchRequest(SoapySSDPEndpointData *data, const SoapyHTTPHeader &header, const std::string &addr);
    void handleSearchResponse(SoapySSDPEndpointData *data, const SoapyHTTPHeader &header, const std::string &addr);
    void handleNotifyRequest(SoapySSDPEndpointData *data, const SoapyHTTPHeader &header, const std::string &addr);
    void handleRegisterService(SoapySSDPEndpointData *, const SoapyHTTPHeader &header, const std::string &recvAddr);
};
