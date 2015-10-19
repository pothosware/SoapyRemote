// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include "SoapyRPCSocket.hpp"
#include <csignal> //sig_atomic_t
#include <vector>

namespace std
{
    class thread;
}

/*!
 * The discovery client runs a background thread
 * to probe for remote devices using SSDP packets.
 */
class SoapyDiscoveryClient
{
public:
    //! Create a discovery client
    SoapyDiscoveryClient(void);

    ~SoapyDiscoveryClient(void);

    //! Get a list of discovered servers
    std::vector<std::string> getServers(void) const;

private:

    SoapyRPCSocket _sock;

    //server handler thread
    std::thread *workerThread;

    //signal done to the thread
    sig_atomic_t done;

    void handlerLoop(void);
};
