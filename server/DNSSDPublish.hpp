// Copyright (c) 2018-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once

struct DNSSDPublishImpl;

/*!
 * The DNS-SD Publisher publishes service
 * information to the system's mDNS daemon.
 */
class DNSSDPublish
{
public:
    //! Connect to the daemon and publish the service
    DNSSDPublish(void);

    //! Disconnect from the daemon
    ~DNSSDPublish(void);

    /*!
     * lets the server loop check client status
     * and restart the connection to the daemon
     *
     * Ex: sudo systemctl restart avahi-daemon
     * will cause a client disconnection and needs to
     * be re-established after the daemon comes back
     */
    void maintenance(void);

private:
    DNSSDPublishImpl *_impl;
};
