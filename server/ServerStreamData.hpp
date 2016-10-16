// Copyright (c) 2015-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include "SoapyRPCSocket.hpp"
#include "ThreadPrioHelper.hpp"
#include <csignal> //sig_atomic_t
#include <string>
#include <thread>

class SoapyStreamEndpoint;

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
class ServerStreamData
{
public:
    ServerStreamData(void);
    ~ServerStreamData(void);

    SoapySDR::Device *device;
    SoapySDR::Stream *stream;
    std::string format;
    size_t chanMask;
    double priority;

    //this ID identifies the stream to the remote host
    int streamId;

    //datagram socket for stream endpoint
    SoapyRPCSocket *streamSock;

    //datagram socket for status endpoint
    SoapyRPCSocket *statusSock;

    //remote side of the stream endpoint
    SoapyStreamEndpoint *endpoint;

    //hooks to start/stop work
    void startSendThread(void);
    void startRecvThread(void);
    void startStatThread(void);
    void stopThreads(void);

    //worker implementations
    void recvEndpointWork(void);
    void sendEndpointWork(void);
    void statEndpointWork(void);

private:
    //worker thread for this stream
    std::thread *streamThread;
    std::thread *statusThread;

    //signal done to the thread
    sig_atomic_t done;
};
