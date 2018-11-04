// Copyright (c) 2015-2018 Josh Blum
// Copyright (c) 2016-2016 Bastille Networks
// SPDX-License-Identifier: BSL-1.0

#include "ServerStreamData.hpp"
#include "SoapyRemoteDefs.hpp"
#include "SoapyStreamEndpoint.hpp"
#include <SoapySDR/Device.hpp>
#include <SoapySDR/Logger.hpp>
#include <algorithm> //min
#include <thread>
#include <vector>
#include <cassert>

template <typename T>
void incrementBuffs(std::vector<T> &buffs, size_t numElems, size_t elemSize)
{
    for (auto &buff : buffs)
    {
        buff = T(size_t(buff) + (numElems*elemSize));
    }
}

ServerStreamData::ServerStreamData(void):
    device(nullptr),
    stream(nullptr),
    chanMask(0),
    priority(0.0),
    streamId(-1),
    streamSock(nullptr),
    statusSock(nullptr),
    endpoint(nullptr),
    streamThread(nullptr),
    statusThread(nullptr),
    done(true)
{
    return;
}

ServerStreamData::~ServerStreamData(void)
{
    delete streamSock;
    delete statusSock;
}

void ServerStreamData::startSendThread(void)
{
    assert(streamId != -1);
    done = false;
    streamThread = new std::thread(&ServerStreamData::sendEndpointWork, this);
}

void ServerStreamData::startRecvThread(void)
{
    assert(streamId != -1);
    done = false;
    streamThread = new std::thread(&ServerStreamData::recvEndpointWork, this);
}

void ServerStreamData::startStatThread(void)
{
    assert(streamId != -1);
    done = false;
    statusThread = new std::thread(&ServerStreamData::statEndpointWork, this);
}

void ServerStreamData::stopThreads(void)
{
    done = true;
    if (streamThread != nullptr)
    {
        streamThread->join();
        delete streamThread;
    }
    if (statusThread != nullptr)
    {
        statusThread->join();
        delete statusThread;
    }
}

static void setThreadPrioWithLogging(const double priority)
{
    const auto errorMsg = setThreadPrio(priority);
    if (not errorMsg.empty()) SoapySDR::logf(SOAPY_SDR_WARNING,
        "Set thread priority %g failed: %s", priority, errorMsg.c_str());
}

void ServerStreamData::recvEndpointWork(void)
{
    setThreadPrioWithLogging(priority);
    assert(endpoint != nullptr);
    assert(endpoint->getElemSize() != 0);
    assert(endpoint->getNumChans() != 0);

    //setup worker data structures
    int ret = 0;
    size_t handle = 0;
    int flags = 0;
    long long timeNs = 0;
    const auto elemSize = endpoint->getElemSize();
    std::vector<const void *> buffs(endpoint->getNumChans());

    //loop forever until signaled done
    //1) wait on the endpoint to become ready
    //2) acquire the recv buffer from the endpoint
    //3) write to the device stream from the endpoint buffer
    //4) release the buffer back to the endpoint
    while (not done)
    {
        if (not endpoint->waitRecv(SOAPY_REMOTE_SOCKET_TIMEOUT_US)) continue;
        ret = endpoint->acquireRecv(handle, buffs.data(), flags, timeNs);
        if (ret < 0)
        {
            SoapySDR::logf(SOAPY_SDR_ERROR, "Server-side receive endpoint: %s; worker quitting...", streamSock->lastErrorMsg());
            return;
        }

        //loop to write to device
        size_t elemsLeft = size_t(ret);
        while (not done)
        {
            ret = device->writeStream(stream, buffs.data(), elemsLeft, flags, timeNs, SOAPY_REMOTE_SOCKET_TIMEOUT_US);
            if (ret == SOAPY_SDR_TIMEOUT) continue;
            if (ret < 0)
            {
                endpoint->writeStatus(ret, chanMask, flags, timeNs);
                break; //discard after error, this may have been invalid flags or time
            }
            if (elemsLeft < (size_t)ret)
            {
                SoapySDR_logf(SOAPY_SDR_ERROR, "Server-side receive endpoint: device->writeStream wrote more elements than requested");
                break; //stop after error
            }
            elemsLeft -= ret;
            incrementBuffs(buffs, ret, elemSize);
            if (elemsLeft == 0) break;
            flags &= ~(SOAPY_SDR_HAS_TIME); //clear time for subsequent writes
        }

        //release the buffer back to the endpoint
        endpoint->releaseRecv(handle);
    }
}

void ServerStreamData::sendEndpointWork(void)
{
    setThreadPrioWithLogging(priority);
    assert(endpoint != nullptr);
    assert(endpoint->getElemSize() != 0);
    assert(endpoint->getNumChans() != 0);

    //setup worker data structures
    int ret = 0;
    size_t handle = 0;
    int flags = 0;
    long long timeNs = 0;
    const auto elemSize = endpoint->getElemSize();
    std::vector<void *> buffs(endpoint->getNumChans());
    const size_t mtuElems = device->getStreamMTU(stream);

    //loop forever until signaled done
    //1) waits on the endpoint to become ready
    //2) acquire the send buffer from the endpoint
    //3) read from the device stream into the endpoint buffer
    //4) release the buffer back to the endpoint (sends)
    while (not done)
    {
        if (not endpoint->waitSend(SOAPY_REMOTE_SOCKET_TIMEOUT_US)) continue;
        ret = endpoint->acquireSend(handle, buffs.data());
        if (ret < 0)
        {
            SoapySDR::logf(SOAPY_SDR_ERROR, "Server-side send endpoint: %s; worker quitting...", streamSock->lastErrorMsg());
            return;
        }

        //Read only up to MTU size with a timeout for minimal waiting.
        //In the next section we will continue the read with non-blocking.
        size_t elemsLeft = size_t(ret);
        size_t elemsRead = 0;
        while (not done)
        {
            flags = 0; //flags is an in/out parameter and must be cleared for consistency
            const size_t numElems = std::min(mtuElems, elemsLeft);
            ret = device->readStream(stream, buffs.data(), numElems, flags, timeNs, SOAPY_REMOTE_SOCKET_TIMEOUT_US);
            if (ret == SOAPY_SDR_TIMEOUT) continue;
            if (ret < 0)
            {
                //ret will be propagated to remote endpoint
                break;
            }
            elemsLeft -= ret;
            elemsRead += ret;
            incrementBuffs(buffs, ret, elemSize);
            break;
        }

        //fill remaining buffer with no timeout
        //This is a latency optimization to forward to the host ASAP,
        //but to use the full bandwidth when more data is available.
        //Do not allow this optimization when end of burst or single packet mode to preserve boundaries
        static const int trailingFlags(SOAPY_SDR_END_BURST | SOAPY_SDR_ONE_PACKET | SOAPY_SDR_END_ABRUPT);
        if (elemsRead != 0 and elemsLeft != 0 and (flags & trailingFlags) == 0)
        {
            int flags1 = 0;
            long long timeNs1 = 0;
            ret = device->readStream(stream, buffs.data(), elemsLeft, flags1, timeNs1, 0);
            if (ret == SOAPY_SDR_TIMEOUT) ret = 0; //timeouts OK
            if (ret > 0)
            {
                elemsLeft -= ret;
                elemsRead += ret;
            }

            //include trailing flags that come from the second read
            flags |= (flags1 & trailingFlags);
        }

        //release the buffer with flags and time from the first read
        //if any read call returned an error, forward the error instead
        endpoint->releaseSend(handle, (ret < 0)?ret:elemsRead, flags, timeNs);
    }
}

void ServerStreamData::statEndpointWork(void)
{
    assert(endpoint != nullptr);

    int ret = 0;
    size_t chanMask = 0;
    int flags = 0;
    long long timeNs = 0;

    while (not done)
    {
        ret = device->readStreamStatus(stream, chanMask, flags, timeNs, SOAPY_REMOTE_SOCKET_TIMEOUT_US);
        if (ret == SOAPY_SDR_TIMEOUT) continue;
        endpoint->writeStatus(ret, chanMask, flags, timeNs);

        //exit the thread if stream status is not supported
        //but only after reporting this to the local endpoint
        if (ret == SOAPY_SDR_NOT_SUPPORTED) return;
    }
}
