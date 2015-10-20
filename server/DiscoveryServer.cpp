// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "DiscoveryServer.hpp"
#include "SoapyURLUtils.hpp"
#include "SoapyInfoUtils.hpp"
#include "SoapyRemoteDefs.hpp"
#include "SoapySSDPUtils.hpp"
#include <thread>
#include <iostream>

SoapyDiscoveryServer::SoapyDiscoveryServer(const std::string &url):
    bindUrl(url),
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
    //int ret = _sock.sendto("hello", 6, groupAddr);
    //std::cout << "send ret=" << ret << " " << _sock.lastErrorMsg() << std::endl;

    workerThread = new std::thread(&SoapyDiscoveryServer::handlerLoop, this);
}

SoapyDiscoveryServer::~SoapyDiscoveryServer(void)
{
    done = true;
    if (workerThread != nullptr) workerThread->join();
    delete workerThread;
}

void SoapyDiscoveryServer::handlerLoop(void)
{
    char buff[1024];
    std::string addr;
    while (not done)
    {
        if (not _sock.selectRecv(SOAPY_REMOTE_SOCKET_TIMEOUT_US)) continue;
        int ret = _sock.recvfrom(buff, sizeof(buff), addr);
        std::cout << "recv ret=" << ret << " " << _sock.lastErrorMsg() << " " << std::string(buff, ret) << std::endl;

        //TODO parse for ST

        auto bindService = SoapyURL(bindUrl).getService();
        auto msg = formatMSearchResponse(SoapyURL("tcp", SoapyInfo::getHostName(), bindService).toString());
        ret = _sock.sendto(msg.data(), msg.size(), addr);
        std::cout << "send ret=" << ret << " " << _sock.lastErrorMsg() << std::endl;
    }
}
