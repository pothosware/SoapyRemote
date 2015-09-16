// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <SoapySDR/Errors.hpp>
#include <SoapySDR/Logger.hpp>
#include "SoapyStreamEndpoint.hpp"
#include "SoapyRPCSocket.hpp"
#include "SoapyRemoteDefs.hpp"
#include "SoapySocketDefs.hpp"
#include <cassert>
#include <cstdint>

#define HEADER_SIZE sizeof(StreamDatagramHeader)

struct StreamDatagramHeader
{
    uint32_t bytes; //!< total number of bytes in datagram
    uint32_t sequence; //!< sequence count for flow control
    uint32_t elems; //!< number of elements or error code
    int flags; //!< flags associated with this datagram
    long long time; //!< time associated with this datagram
};

SoapyStreamEndpoint::SoapyStreamEndpoint(
    SoapyRPCSocket &sock,
    const bool isRecv,
    const size_t numChans,
    const size_t elemSize,
    const size_t mtu,
    const size_t window):
    _sock(sock),
    _numChans(numChans),
    _elemSize(elemSize),
    _buffSize(mtu/numChans/elemSize),
    _numBuffs(SOAPY_REMOTE_ENDPOINT_NUM_BUFFS),
    _nextHandleAcquire(0),
    _nextHandleRelease(0),
    _numHandlesAcquired(0),
    _nextSequenceNumber(0)
{
    assert(not _sock.null());

    //allocate buffer data and default state
    _buffData.resize(_numBuffs);
    for (auto &data : _buffData)
    {
        data.acquired = false;
        data.buff.resize(HEADER_SIZE+mtu);
        data.buffs.resize(_numChans);
        for (size_t i = 0; i < _numChans; i++)
        {
            size_t offsetBytes = HEADER_SIZE+(i*mtu/numChans);
            data.buffs[i] = (void*)(data.buff.data()+offsetBytes);
        }
    }

    //receive endpoints require a large receive buffer
    //so that the socket can buffer the incoming stream data
    int actualWindow = 0;
    if (isRecv)
    {
        actualWindow = _sock.setRecvBuffSize(window);
    }
    //send endpoints require a large send buffer
    //so that the socket can buffer the outgoing stream data
    else
    {
        actualWindow = _sock.setSendBuffSize(window);
    }

    //log when the size is not expected
    //users may have to tweak system parameters
    if (actualWindow < 0)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "StreamEndpoint resize socket buffer FAIL: %s", _sock.lastErrorMsg());
    }
    else if (size_t(actualWindow) < window)
    {
        SoapySDR::logf(SOAPY_SDR_WARNING, "StreamEndpoint resize socket buffer: set %d bytes, got %d bytes", int(window), int(actualWindow));
    }
}

SoapyStreamEndpoint::~SoapyStreamEndpoint(void)
{
    return;
}

bool SoapyStreamEndpoint::waitRecv(const long timeoutUs)
{
    return _sock.selectRecv(timeoutUs);
}

int SoapyStreamEndpoint::acquireRecv(size_t &handle, const void **buffs, int &flags, long long &timeNs)
{
    //no available handles, the user is hoarding them...
    if (_numHandlesAcquired == _buffData.size()) return SOAPY_SDR_STREAM_ERROR;

    //grab the current handle
    handle = _nextHandleAcquire;
    auto &data = _buffData[handle];

    //receive into the buffer
    assert(not _sock.null());
    int ret = _sock.recv(data.buff.data(), data.buff.size());
    if (ret < 0) return SOAPY_SDR_STREAM_ERROR;

    //check the header
    auto header = (const StreamDatagramHeader*)data.buff.data();
    size_t bytes = ntohl(header->bytes);
    if (bytes > size_t(ret))
    {
        //TODO truncation, warn to user about MTU
        return SOAPY_SDR_STREAM_ERROR;
    }

    //TODO sequence failures
    //TODO update flow control

    //increment for next handle
    data.acquired = true;
    _nextHandleAcquire = (_nextHandleAcquire + 1)%_numBuffs;
    _numHandlesAcquired++;

    //set output parameters
    this->getAddrs(handle, (void **)buffs);
    flags = ntohl(header->flags);
    timeNs = ntohll(header->time);
    return int(ntohl(header->elems));
}

void SoapyStreamEndpoint::releaseRecv(const size_t handle)
{
    auto &data = _buffData[handle];
    data.acquired = false;

    //actually release in order of handle index
    while (_numHandlesAcquired != 0)
    {
        if (_buffData[_nextHandleRelease].acquired) break;
        _nextHandleRelease = (_nextHandleRelease + 1)%_numBuffs;
        _numHandlesAcquired--;
    }
}

bool SoapyStreamEndpoint::waitSend(const long timeoutUs)
{
    //TODO check and or wait on flow control
    return true; //always ready for now
}

int SoapyStreamEndpoint::acquireSend(size_t &handle, void **buffs)
{
    //no available handles, the user is hoarding them...
    if (_numHandlesAcquired == _buffData.size()) return SOAPY_SDR_STREAM_ERROR;

    //grab the current handle
    handle = _nextHandleAcquire;
    auto &data = _buffData[handle];

    //increment for next handle
    data.acquired = true;
    _nextHandleAcquire = (_nextHandleAcquire + 1)%_numBuffs;
    _numHandlesAcquired++;

    //set output parameters
    this->getAddrs(handle, buffs);
    return int(_buffSize);
}

void SoapyStreamEndpoint::releaseSend(const size_t handle, const int numElemsOrErr, int &flags, const long long timeNs)
{
    auto &data = _buffData[handle];
    data.acquired = false;

    //load the header
    auto header = (StreamDatagramHeader*)data.buff.data();
    size_t bytes = HEADER_SIZE + ((numElemsOrErr < 0)?0:(numElemsOrErr*_elemSize));
    header->bytes = htonl(bytes);
    header->sequence = htonl(_nextSequenceNumber++);
    header->elems = htonl(numElemsOrErr);
    header->flags = htonl(flags);
    header->time = htonll(timeNs);

    //send from the buffer
    assert(not _sock.null());
    int ret = _sock.send(data.buff.data(), bytes);
    if (ret < 0)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "StreamEndpoint::releaseSend(), FAILED %s", _sock.lastErrorMsg());
    }
    else if (size_t(ret) != bytes)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "StreamEndpoint::releaseSend(%d bytes), FAILED %d", int(bytes), ret);
    }

    //actually release in order of handle index
    while (_numHandlesAcquired != 0)
    {
        if (_buffData[_nextHandleRelease].acquired) break;
        _nextHandleRelease = (_nextHandleRelease + 1)%_numBuffs;
        _numHandlesAcquired--;
    }
}
