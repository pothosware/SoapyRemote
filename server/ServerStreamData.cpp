// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "ServerStreamData.hpp"
#include "SoapyRemoteDefs.hpp"
#include "SoapyRecvEndpoint.hpp"
#include "SoapySendEndpoint.hpp"
#include <SoapySDR/Device.hpp>
#include <SoapySDR/Logger.hpp>
#include <cctype> //isdigit
#include <vector>

static size_t formatToSize(const std::string &format)
{
    size_t size = 0;
    size_t isComplex = false;
    for (const char ch : format)
    {
        if (ch == 'C') isComplex = true;
        if (std::isdigit(ch)) size = (size*10) + size_t(ch-'0');
    }
    size /= 8; //bits to bytes
    return isComplex?size*2:size;
}

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
    recvEndpoint(nullptr),
    sendEndpoint(nullptr),
    done(true)
{
    return;
}

void ServerStreamData::startThread(void)
{
    done = false;
    if (recvEndpoint != nullptr)
    {
        workerThread = std::thread(&ServerStreamData::recvEndpointWork, this);
    }
    if (sendEndpoint != nullptr)
    {
        workerThread = std::thread(&ServerStreamData::sendEndpointWork, this);
    }
}

void ServerStreamData::stopThread(void)
{
    done = true;
    workerThread.join();
}

void ServerStreamData::recvEndpointWork(void)
{
    //setup worker data structures
    int ret = 0;
    size_t handle = 0;
    int flags = 0;
    long long timeNs = 0;
    const auto elemSize = formatToSize(format);
    std::vector<const void *> buffs(recvEndpoint->getNumChans());

    //loop forever until signaled done
    //1) wait on the endpoint to become ready
    //2) acquire the recv buffer from the endpoint
    //3) write to the device stream from the endpoint buffer
    //4) release the buffer back to the endpoint
    while (not done)
    {
        if (not recvEndpoint->wait(SOAPY_REMOTE_SOCKET_TIMEOUT_US)) continue;
        ret = recvEndpoint->acquire(handle, buffs.data(), flags, timeNs);
        if (ret < 0)
        {
            SoapySDR::logf(SOAPY_SDR_ERROR, "Server-side receive endpoint: %s; worker quitting...", sock.lastErrorMsg());
            return;
        }

        //loop to write to device
        size_t elemsLeft = size_t(ret)/elemSize; //bytes to elements
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
        recvEndpoint->release(handle);
    }
}

void ServerStreamData::sendEndpointWork(void)
{
    //setup worker data structures
    int ret = 0;
    size_t handle = 0;
    int flags = 0;
    long long timeNs = 0;
    const auto elemSize = formatToSize(format);
    std::vector<void *> buffs(recvEndpoint->getNumChans());

    //loop forever until signaled done
    //1) waits on the endpoint to become ready
    //2) acquire the send buffer from the endpoint
    //3) read from the device stream into the endpoint buffer
    //4) release the buffer back to the endpoint (sends)
    while (not done)
    {
        if (not sendEndpoint->wait(SOAPY_REMOTE_SOCKET_TIMEOUT_US)) continue;
        ret = sendEndpoint->acquire(handle, buffs.data());
        if (ret < 0)
        {
            SoapySDR::logf(SOAPY_SDR_ERROR, "Server-side send endpoint: %s; worker quitting...", sock.lastErrorMsg());
            return;
        }

        //loop to read from device
        size_t elemsLeft = size_t(ret);
        size_t elemsRead = 0;
        while (not done)
        {
            ret = device->readStream(stream, buffs.data(), elemsLeft, flags, timeNs, SOAPY_REMOTE_SOCKET_TIMEOUT_US);
            if (ret == SOAPY_SDR_TIMEOUT) continue;
            if (ret < 0)
            {
                //ret will be propagated to remote endpoint
                break;
            }
            elemsLeft -= ret;
            elemsRead += ret;
            incrementBuffs(buffs, ret, elemSize);
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
        const int bytesRead = int(elemsRead*elemSize); //elements to bytes
        sendEndpoint->release(handle, (ret < 0)?ret:bytesRead, flags, timeNs);
    }
}
