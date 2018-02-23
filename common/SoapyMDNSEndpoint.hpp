// Copyright (c) 2018-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include "SoapyRemoteConfig.hpp"
#include <string>
#include <map>

#define SOAPY_REMOTE_DNSSD_TYPE "_soapy._tcp"

struct SoapyMDNSEndpointData;

/*!
 * The DNS-SD client ties into the system's mDNS daemon.
 * Used for both server side for publishing,
 * and the client side for browsing/lookup.
 */
class SOAPY_REMOTE_API SoapyMDNSEndpoint
{
public:

    //! Connect to the daemon
    SoapyMDNSEndpoint(void);

    //! Disconnect from the daemon
    ~SoapyMDNSEndpoint(void);

    //! Print information about the client
    void printInfo(void);

    //! Is the client connected and operational?
    bool status(void);

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
    SoapyMDNSEndpointData *_impl;
};
