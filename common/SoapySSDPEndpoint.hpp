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
 * Service an SSDP endpoint to:
 * keep track of discovered servers of interest,
 * and to respond to discovery packets for us.
 */
class SoapySSDPEndpoint
{
public:
    /*!
     * Create a discovery server
     * \param ipVersion version number 4 or 6
     */
    SoapySSDPEndpoint(const int ipVersion);

    ~SoapySSDPEndpoint(void);

    /*!
     * Allow the endpoint to advertise that its running the RPC service
     */
    void advertiseService(const std::string &service)
    {
        this->service = service;
    }

    /*!
     * Enable the client endpoint to search for running services.
     */
    void enablePeriodicSearch(void);

    /*!
     * Enable the server to send periodic notification messages.
     */
    void enablePeriodicNotify(void);

private:
    //multi-cast group URL
    std::string groupURL;

    SoapyRPCSocket sock;

    //name of the service port to advertise
    std::string service;

    //server handler thread
    std::thread *workerThread;

    //signal done to the thread
    sig_atomic_t done;

    void handlerLoop(void);
};
