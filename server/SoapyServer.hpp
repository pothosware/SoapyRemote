// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <csignal> //sig_atomic_t
#include <thread>
#include <map>

class SoapyRPCSocket;

//! Client handler data
struct SoapyServerThreadData
{
    SoapyServerThreadData(void);
    ~SoapyServerThreadData(void);
    void handlerLoop(void);
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
