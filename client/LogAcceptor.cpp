// Copyright (c) 2015-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "LogAcceptor.hpp"
#include "SoapyRemoteDefs.hpp"
#include "SoapyRPCSocket.hpp"
#include "SoapyRPCPacker.hpp"
#include "SoapyRPCUnpacker.hpp"
#include <SoapySDR/Logger.hpp>
#include <csignal> //sig_atomic_t
#include <cassert>
#include <mutex>
#include <thread>
#include <map>

/***********************************************************************
 * Log acceptor thread implementation
 **********************************************************************/
struct LogAcceptorThreadData
{
    LogAcceptorThreadData(void):
        timeoutUs(SOAPY_REMOTE_SOCKET_TIMEOUT_US),
        done(true),
        thread(nullptr),
        useCount(0)
    {
        return;
    }

    ~LogAcceptorThreadData(void)
    {
        this->shutdown();
    }

    void activate(void);

    void shutdown(void);

    void handlerLoop(void);

    SoapyRPCSocket client;
    std::string url;
    long timeoutUs;
    sig_atomic_t done;
    std::thread *thread;
    sig_atomic_t useCount;
};

void LogAcceptorThreadData::activate(void)
{
    client = SoapyRPCSocket();
    //specify a timeout on connect because the link may be lost
    //when the thread attempts to re-establish a connection
    int ret = client.connect(url, timeoutUs);
    if (ret != 0)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "SoapyLogAcceptor::connect(%s) FAIL: %s", url.c_str(), client.lastErrorMsg());
        done = true;
        return;
    }

    try
    {
        //startup forwarding
        SoapyRPCPacker packerStart(client);
        packerStart & SOAPY_REMOTE_START_LOG_FORWARDING;
        packerStart();
        SoapyRPCUnpacker unpackerStart(client, true, timeoutUs);
        done = false;
        thread = new std::thread(&LogAcceptorThreadData::handlerLoop, this);
    }
    catch (const std::exception &ex)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "SoapyLogAcceptor::activate(%s) FAIL: %s", url.c_str(), ex.what());
        done = true;
    }
}

void LogAcceptorThreadData::shutdown(void)
{
    try
    {
        //shutdown forwarding (ignore reply)
        SoapyRPCPacker packerStop(client);
        packerStop & SOAPY_REMOTE_STOP_LOG_FORWARDING;
        packerStop();

        //graceful disconnect (ignore reply)
        SoapyRPCPacker packerHangup(client);
        packerHangup & SOAPY_REMOTE_HANGUP;
        packerHangup();
    }
    catch (const std::exception &ex)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "SoapyLogAcceptor::shutdown(%s) FAIL: %s", url.c_str(), ex.what());
    }

    //the thread will exit due to the requests above
    assert(thread != nullptr);
    thread->join();
    delete thread;
    done = true;
    client = SoapyRPCSocket();
}

void LogAcceptorThreadData::handlerLoop(void)
{
    try
    {
        //loop while active to relay messages to logger
        while (true)
        {
            SoapyRPCUnpacker unpackerLogMsg(client, true, -1/*no timeout*/);
            if (unpackerLogMsg.done()) break; //got stop reply
            char logLevel = 0;
            std::string message;
            unpackerLogMsg & logLevel;
            unpackerLogMsg & message;
            SoapySDR::log(SoapySDR::LogLevel(logLevel), message);
        }
    }
    catch (const std::exception &ex)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "SoapyLogAcceptor::handlerLoop(%s) FAIL: %s", url.c_str(), ex.what());
    }

    done = true;
}

/***********************************************************************
 * log acceptor threads and subscription tracking
 **********************************************************************/
static std::mutex logMutex;

//unique server id to thread data
static std::map<std::string, LogAcceptorThreadData> handlers;

//cleanup completed and restart on errors
static void threadMaintenance(void)
{
    auto it = handlers.begin();
    while (it != handlers.end())
    {
        auto &data = it->second;

        //first time, or error occurred
        if (data.done) data.activate();

        //no subscribers, erase
        if (data.useCount == 0) handlers.erase(it++);

        //next element
        else ++it;
    }
}

/***********************************************************************
 * client subscription hooks
 **********************************************************************/
SoapyLogAcceptor::SoapyLogAcceptor(const std::string &url, SoapyRPCSocket &sock, const long timeoutUs)
{
    SoapyRPCPacker packer(sock);
    packer & SOAPY_REMOTE_GET_SERVER_ID;
    packer();
    SoapyRPCUnpacker unpacker(sock, true, timeoutUs);
    unpacker & _serverId;

    std::lock_guard<std::mutex> lock(logMutex);

    auto &data = handlers[_serverId];
    data.useCount++;
    data.url = url;
    if (timeoutUs != 0) data.timeoutUs = timeoutUs;

    threadMaintenance();
}

SoapyLogAcceptor::~SoapyLogAcceptor(void)
{
    std::lock_guard<std::mutex> lock(logMutex);

    auto &data = handlers.at(_serverId);
    data.useCount--;

    threadMaintenance();
}
