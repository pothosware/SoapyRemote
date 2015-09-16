// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include "SoapyRPCSocket.hpp"
#include <csignal> //sig_atomic_t
#include <string>
#include <thread>

class SoapyRecvEndpoint;
class SoapySendEndpoint;

namespace SoapySDR
{
    class Device;
    class Stream;
}

/*!
 * Server-side stream data for client handler.
 * This class manages a recv/send endpoint,
 * and a thread to handle that endpoint.
 */
struct ServerStreamData
{
    ServerStreamData(void);

    SoapySDR::Device *device;
    SoapySDR::Stream *stream;
    std::string format;

    //this ID identifies the stream to the remote host
    int streamId;

    //datagram socket for stream endpoint
    SoapyRPCSocket sock;

    //using one of the following endpoints
    SoapyRecvEndpoint *recvEndpoint;
    SoapySendEndpoint *sendEndpoint;

    //worker thread for this stream
    std::thread workerThread;

    //signal done to the thread
    sig_atomic_t done;

    //hooks to start/stop work
    void startThread(void);
    void stopThread(void);

    //worker implementations
    void recvEndpointWork(void);
    void sendEndpointWork(void);
};
