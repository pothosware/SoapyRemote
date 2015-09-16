// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "ServerStreamData.hpp"
#include "SoapyRemoteDefs.hpp"
#include "SoapyStreamEndpoint.hpp"
#include <SoapySDR/Device.hpp>
#include <SoapySDR/Logger.hpp>
#include <algorithm> //min
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
    streamId(-1),
    endpoint(nullptr),
    done(true)
{
    return;
}

void ServerStreamData::startSendThread(void)
{
    assert(streamId != -1);
    done = false;
    workerThread = std::thread(&ServerStreamData::sendEndpointWork, this);
}

void ServerStreamData::startRecvThread(void)
{
    assert(streamId != -1);
    done = false;
    workerThread = std::thread(&ServerStreamData::recvEndpointWork, this);
}

void ServerStreamData::stopThread(void)
{
    done = true;
    workerThread.join();
}

void ServerStreamData::recvEndpointWork(void)
{
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
            SoapySDR::logf(SOAPY_SDR_ERROR, "Server-side receive endpoint: %s; worker quitting...", sock.lastErrorMsg());
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
                //TODO propagate error to async status...
                break; //discard after error, this may have been invalid flags or time
            }
            elemsLeft -= ret;
            incrementBuffs(buffs, ret, elemSize);
            if (elemsLeft == 0) break;
        }

        //release the buffer back to the endpoint
        endpoint->releaseRecv(handle);
    }
}

void ServerStreamData::sendEndpointWork(void)
{
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
            SoapySDR::logf(SOAPY_SDR_ERROR, "Server-side send endpoint: %s; worker quitting...", sock.lastErrorMsg());
            return;
        }

        //Read only up to MTU size with a timeout for minimal waiting.
        //In the next section we will continue the read with non-blocking.
        size_t elemsLeft = size_t(ret);
        size_t elemsRead = 0;
        while (not done)
        {
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
        if (elemsRead != 0 and elemsLeft != 0)
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
        }

        //release the buffer with flags and time from the first read
        //if any read call returned an error, forward the error instead
        endpoint->releaseSend(handle, (ret < 0)?ret:elemsRead, flags, timeNs);
    }
}
