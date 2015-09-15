// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "LogForwarding.hpp"
#include "SoapyRemoteDefs.hpp"
#include "SoapyRPCPacker.hpp"
#include <SoapySDR/Logger.hpp>
#include <mutex>
#include <set>

/***********************************************************************
 * socket subscribers for log forwarding
 **********************************************************************/
static std::mutex subscribersMutex;

static std::set<SoapyRPCSocket *> subscribers;

/***********************************************************************
 * custom log handling
 **********************************************************************/
static void handleLogMessage(const SoapySDRLogLevel logLevel, const char *message)
{
    std::string strMessage(message);

    std::lock_guard<std::mutex> lock(subscribersMutex);
    for (auto sock : subscribers)
    {
        try
        {
            SoapyRPCPacker packer(*sock);
            packer & char(logLevel);
            packer & strMessage;
            packer();
        }
        catch (...)
        {
            //ignored
        }
    }
}

/***********************************************************************
 * subscriber reregistration entry points
 **********************************************************************/
SoapyLogForwarder::SoapyLogForwarder(SoapyRPCSocket &sock):
    _sock(sock)
{
    std::lock_guard<std::mutex> lock(subscribersMutex);
    subscribers.insert(&_sock);

    //register the log handler, its safe to re-register every time
    SoapySDR::registerLogHandler(&handleLogMessage);
}

SoapyLogForwarder::~SoapyLogForwarder(void)
{
    std::lock_guard<std::mutex> lock(subscribersMutex);
    subscribers.erase(&_sock);
}
