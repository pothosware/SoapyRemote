// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "DiscoveryClient.hpp"
#include "SoapyURLUtils.hpp"
#include "SoapyRemoteDefs.hpp"
#include "SoapySSDPUtils.hpp"
#include <thread>
#include <iostream>

SoapyDiscoveryClient::SoapyDiscoveryClient(void):
    workerThread(nullptr),
    done(false)
{
    //std::string groupAddr("[ff02::c]:1900");
    //std::string localAddr("udp://[::]:1900");

    std::string groupAddr("[239.255.255.250]:1900");
    std::string localAddr("udp://[0.0.0.0]:1900");

    //this is a test...
    _sock.multicastJoin(groupAddr);
    std::cout << _sock.lastErrorMsg() << std::endl;
    _sock.bind(localAddr);
    std::cout << _sock.lastErrorMsg() << std::endl;

    //send a test message
    auto msg = formatMSearchRequest(groupAddr);
    int ret = _sock.sendto(msg.data(), msg.size(), groupAddr);
    std::cout << "send ret=" << ret << " " << _sock.lastErrorMsg() << std::endl;

    workerThread = new std::thread(&SoapyDiscoveryClient::handlerLoop, this);
}

SoapyDiscoveryClient::~SoapyDiscoveryClient(void)
{
    done = true;
    if (workerThread != nullptr) workerThread->join();
    delete workerThread;
}

void SoapyDiscoveryClient::handlerLoop(void)
{
    while (not done)
    {
        
    }
}
