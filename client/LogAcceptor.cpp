// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "LogAcceptor.hpp"
#include "SoapyRPCPacker.hpp"
#include "SoapyRPCUnpacker.hpp"
#include <SoapySDR/Logger.hpp>
#include <csignal> //sig_atomic_t
#include <mutex>
#include <thread>
#include <map>

/***********************************************************************
 * shared data structure for each acceptor thread
 **********************************************************************/
struct LogAcceptorThreadData
{
    LogAcceptorThreadData(void):
        done(true),
        useCount(0)
    {
        return;
    }
    std::string url;
    sig_atomic_t done;
    std::thread thread;
    sig_atomic_t useCount;
};

/***********************************************************************
 * unpack into the logger call
 **********************************************************************/
void postLogMessage(SoapyRPCUnpacker &unpacker)
{
    char logLevel = 0;
    std::string message;
    unpacker & logLevel;
    unpacker & message;
    SoapySDR::log(SoapySDR::LogLevel(logLevel), message);
}

/***********************************************************************
 * log acceptor thread handler loop
 **********************************************************************/
static void handleLogAcceptor(LogAcceptorThreadData *data)
{
    SoapyRPCSocket s;
    int ret = s.connect(data->url);
    if (ret != 0)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "SoapyLogAcceptor::handlerLoop() -- connect FAIL: %s", s.lastErrorMsg());
        data->done = true;
        return;
    }

    try
    {
        //startup forwarding
        SoapyRPCPacker packerStart(s);
        packerStart & SOAPY_REMOTE_START_LOG_FORWARDING;
        packerStart();
        SoapyRPCUnpacker unpackerStart(s);

        //loop while active to relay messages to logger
        while (data->useCount != 0)
        {
            if (not s.selectRecv(SOAPY_REMOTE_SOCKET_TIMEOUT_US)) continue;
            SoapyRPCUnpacker unpackerLogMsg(s);
            postLogMessage(unpackerLogMsg);
        }

        //shutdown forwarding
        SoapyRPCPacker packerStop(s);
        packerStop & SOAPY_REMOTE_STOP_LOG_FORWARDING;
        packerStop();

        //wait for stop reply -- possible log messages in transit
        while (true)
        {
            SoapyRPCUnpacker unpackerStop(s);
            if (unpackerStop.done()) break; //its the stop reply
            postLogMessage(unpackerStop); //its a log message
        }

        //graceful disconnect
        SoapyRPCPacker packerHangup(s);
        packerHangup & SOAPY_REMOTE_HANGUP;
        packerHangup();
        SoapyRPCUnpacker unpackerHangup(s);

    }
    catch (const std::exception &ex)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "SoapyLogAcceptor::handlerLoop() ", ex.what());
    }

    data->done = true;
}

/***********************************************************************
 * log acceptor threads and subscription tracking
 **********************************************************************/
static std::mutex logMutex;

//unique server id to thread data
static std::map<std::string, LogAcceptorThreadData> _handlers;

//cleanup completed and restart on errors
static void threadMaintenance(void)
{
    auto it = _handlers.begin();
    while (it != _handlers.end())
    {
        auto &data = it->second;

        //no subscribers, erase
        if (data.useCount == 0)
        {
            data.thread.join();
            _handlers.erase(it++);
            continue;
        }

        //restart thread
        if (data.done)
        {
            data.done = false;
            data.thread = std::thread(&handleLogAcceptor, &data);
        }

        //continue to next
        ++it;

    }
}

/***********************************************************************
 * client subscription hooks
 **********************************************************************/
SoapyLogAcceptor::SoapyLogAcceptor(const std::string &url, SoapyRPCSocket &sock)
{
    SoapyRPCPacker packer(sock);
    packer & SOAPY_REMOTE_GET_SERVER_ID;
    packer();
    SoapyRPCUnpacker unpacker(sock);
    unpacker & _serverId;

    std::lock_guard<std::mutex> lock(logMutex);

    auto &data = _handlers[_serverId];
    data.useCount++;
    data.url = url;

    threadMaintenance();
}

SoapyLogAcceptor::~SoapyLogAcceptor(void)
{
    std::lock_guard<std::mutex> lock(logMutex);

    auto &data = _handlers[_serverId];
    data.useCount--;

    threadMaintenance();
}
