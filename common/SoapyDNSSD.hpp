// Copyright (c) 2018-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <string>
#include <vector>
#include <memory>

struct SoapyDNSSDImpl;

/*!
 * The DNS-SD client ties into the system's mDNS daemon.
 * Used for both server side for publishing,
 * and the client side for browsing/lookup.
 */
class SoapyDNSSD
{
public:

    //! Get a singleton instance of the endpoint
    static std::shared_ptr<SoapyDNSSD> getInstance(void);

    //! Connect to the daemon
    SoapyDNSSD(void);

    //! Disconnect from the daemon
    ~SoapyDNSSD(void);

    //! Print information about the client
    void printInfo(void);

    /*!
     * Allow the endpoint to advertise that its running the RPC service
     */
    void registerService(const std::string &uuid, const std::string &service);

    /*!
     * lets the server loop check client status
     * and restart the connection to the daemon
     *
     * Ex: sudo systemctl restart avahi-daemon
     * will cause a client disconnection and needs to
     * be re-established after the daemon comes back
     */
    void maintenance(void);

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
    std::string _uuid, _service;
    SoapyDNSSDImpl *_impl;
};
