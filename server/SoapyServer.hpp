// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include "SoapyRPCSocket.hpp"
#include "SoapyRPCPacker.hpp"
#include "SoapyRPCUnpacker.hpp"
#include <csignal> //sig_atomic_t
#include <thread>
#include <map>

class SoapyLogForwarder;

namespace SoapySDR
{
    class Device;
}

/*!
 * The client handler manages a remote client.
 */
class SoapyClientHandler
{
public:
    SoapyClientHandler(SoapyRPCSocket &sock);

    ~SoapyClientHandler(void);

    //handle once, return true to call again
    bool handleOnce(void);

private:
    bool handleOnce(SoapyRPCUnpacker &unpacker, SoapyRPCPacker &packer);

    SoapyRPCSocket &_sock;
    SoapySDR::Device *_dev;
    SoapyLogForwarder *_logForwarder;
};

//! Client handler data
struct SoapyServerThreadData
{
    sig_atomic_t done;
    std::thread thread;
    SoapyRPCSocket *client;
};

/*!
 * The server listener class accepts clients and spawns threads.
 */
class SoapyServerListener
{
public:
    SoapyServerListener(SoapyRPCSocket &sock);

    ~SoapyServerListener(void);

    void handleOnce(void);

private:
    SoapyRPCSocket &_sock;
    size_t _handlerId;
    std::map<size_t, SoapyServerThreadData> _handlers;
};
