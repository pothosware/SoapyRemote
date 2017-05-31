// Copyright (c) 2015-2017 Josh Blum
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
    sig_atomic_t done;
    std::thread *thread;
    sig_atomic_t useCount;
};

void LogAcceptorThreadData::activate(void)
{
    client = SoapyRPCSocket();
    int ret = client.connect(url);
    if (ret != 0)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "SoapyLogAcceptor::connect() FAIL: %s", client.lastErrorMsg());
        done = true;
        return;
    }

    try
    {
        //startup forwarding
        SoapyRPCPacker packerStart(client);
        packerStart & SOAPY_REMOTE_START_LOG_FORWARDING;
        packerStart();
        SoapyRPCUnpacker unpackerStart(client);
        done = false;
        thread = new std::thread(&LogAcceptorThreadData::handlerLoop, this);
    }
    catch (const std::exception &ex)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "SoapyLogAcceptor::activate() FAIL: %s", ex.what());
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
        SoapySDR::logf(SOAPY_SDR_ERROR, "SoapyLogAcceptor::shutdown() FAIL: %s", ex.what());
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
            SoapyRPCUnpacker unpackerLogMsg(client);
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
        SoapySDR::logf(SOAPY_SDR_ERROR, "SoapyLogAcceptor::handlerLoop() FAIL: %s", ex.what());
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
SoapyLogAcceptor::SoapyLogAcceptor(const std::string &url, SoapyRPCSocket &sock)
{
    SoapyRPCPacker packer(sock);
    packer & SOAPY_REMOTE_GET_SERVER_ID;
    packer();
    SoapyRPCUnpacker unpacker(sock);
    unpacker & _serverId;

    std::lock_guard<std::mutex> lock(logMutex);

    auto &data = handlers[_serverId];
    data.useCount++;
    data.url = url;

    threadMaintenance();
}

SoapyLogAcceptor::~SoapyLogAcceptor(void)
{
    std::lock_guard<std::mutex> lock(logMutex);

    auto &data = handlers.at(_serverId);
    data.useCount--;

    threadMaintenance();
}
