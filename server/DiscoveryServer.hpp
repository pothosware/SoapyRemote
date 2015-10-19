// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include "SoapyRPCSocket.hpp"
#include <csignal> //sig_atomic_t

namespace std
{
    class thread;
}

/*!
 * The discovery server runs a background thread
 * to handle multi-cast SSDP discovery packets.
 */
class SoapyDiscoveryServer
{
public:
    //! Create a discovery server with the server's bind URL
    SoapyDiscoveryServer(const std::string &url);

    ~SoapyDiscoveryServer(void);

private:

    std::string bindUrl;
    SoapyRPCSocket _sock;

    //server handler thread
    std::thread *workerThread;

    //signal done to the thread
    sig_atomic_t done;

    void handlerLoop(void);
};
