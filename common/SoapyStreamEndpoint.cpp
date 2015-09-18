// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <SoapySDR/Errors.hpp>
#include <SoapySDR/Logger.hpp>
#include "SoapyStreamEndpoint.hpp"
#include "SoapyRPCSocket.hpp"
#include "SoapyURLUtils.hpp"
#include "SoapyRemoteDefs.hpp"
#include "SoapySocketDefs.hpp"
#include <cassert>
#include <cstdint>

#define HEADER_SIZE sizeof(StreamDatagramHeader)

static size_t protocolHeaderSize(SoapyRPCSocket &sock)
{
    struct sockaddr_storage addr;
    int addrlen = sizeof(addr);
    std::string errorMsg;
    lookupURL(sock.getsockname(), addr, addrlen);
    if (addr.ss_family == AF_INET) return 20 + 8;
    else return 40 + 8; //IPv6
}

struct StreamDatagramHeader
{
    uint32_t bytes; //!< total number of bytes in datagram
    uint32_t sequence; //!< sequence count for flow control
    uint32_t elems; //!< number of elements or error code
    int flags; //!< flags associated with this datagram
    long long time; //!< time associated with this datagram
};

SoapyStreamEndpoint::SoapyStreamEndpoint(
    SoapyRPCSocket &streamSock,
    SoapyRPCSocket &statusSock,
    const bool isRecv,
    const size_t numChans,
    const size_t elemSize,
    const size_t mtu,
    const size_t window):
    _streamSock(streamSock),
    _statusSock(statusSock),
    _xferSize(mtu-protocolHeaderSize(streamSock)),
    _numChans(numChans),
    _elemSize(elemSize),
    _buffSize(((_xferSize-HEADER_SIZE)/numChans)/elemSize),
    _numBuffs(SOAPY_REMOTE_ENDPOINT_NUM_BUFFS),
    _nextHandleAcquire(0),
    _nextHandleRelease(0),
    _numHandlesAcquired(0),
    _lastSendSequence(0),
    _lastRecvSequence(0),
    _maxInFlightSeqs(0),
    _triggerAckWindow(0)
{
    assert(not _streamSock.null());

    //allocate buffer data and default state
    _buffData.resize(_numBuffs);
    for (auto &data : _buffData)
    {
        data.acquired = false;
        data.buff.resize(_xferSize);
        data.buffs.resize(_numChans);
        for (size_t i = 0; i < _numChans; i++)
        {
            size_t offsetBytes = HEADER_SIZE+(i*_buffSize*_elemSize);
            data.buffs[i] = (void*)(data.buff.data()+offsetBytes);
        }
    }

    //receive endpoints require a large receive buffer
    //so that the socket can buffer the incoming stream data
    int actualWindow = 0;
    if (isRecv)
    {
        actualWindow = _streamSock.setRecvBuffSize(window);
    }
    //send endpoints require a large send buffer
    //so that the socket can buffer the outgoing stream data
    else
    {
        actualWindow = _streamSock.setSendBuffSize(window);
    }

    //log when the size is not expected
    //users may have to tweak system parameters
    if (actualWindow < 0)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "StreamEndpoint resize socket buffer FAIL: %s", _streamSock.lastErrorMsg());
        actualWindow = window;
    }
    else if (size_t(actualWindow) < window)
    {
        SoapySDR::logf(SOAPY_SDR_WARNING, "StreamEndpoint resize socket buffer: set %d bytes, got %d bytes", int(window), int(actualWindow));
    }

    //calculate maximum in-flight sequences allowed
    _maxInFlightSeqs = actualWindow/mtu;

    //calculate the flow control ACK conditions
    _triggerAckWindow = _maxInFlightSeqs/SOAPY_REMOTE_ENDPOINT_NUM_BUFFS;

    //print summary
    SoapySDR::logf(SOAPY_SDR_INFO, "Configured %s endpoint: dgram=%d bytes, %d elements @ %d bytes, window=%d KiB",
        isRecv?"receiver":"sender", int(_xferSize), int(_buffSize*_numChans), int(_elemSize), int(actualWindow/1024));
}

SoapyStreamEndpoint::~SoapyStreamEndpoint(void)
{
    return;
}

void SoapyStreamEndpoint::sendACK(void)
{
    //has there been at least trigger window number of sequences since the last ACK?
    //or have we reached the trigger timeout maximum duration since the last ACK?
    if (uint32_t(_lastSendSequence+_triggerAckWindow) < uint32_t(_lastRecvSequence))
    {
        StreamDatagramHeader header;
        header.bytes = htonl(sizeof(header));
        header.sequence = htonl(_lastRecvSequence);
        header.elems = htonl(0);
        header.flags = htonl(0);
        header.time = htonll(0);

        //send the flow control ACK
        int ret = _streamSock.send(&header, sizeof(header));
        if (ret < 0)
        {
            SoapySDR::logf(SOAPY_SDR_ERROR, "StreamEndpoint::sendACK(), FAILED %s", _streamSock.lastErrorMsg());
        }
        else if (size_t(ret) != sizeof(header))
        {
            SoapySDR::logf(SOAPY_SDR_ERROR, "StreamEndpoint::sendACK(%d bytes), FAILED %d", int(sizeof(header)), ret);
        }

        //update last flow control ACK state
        _lastSendSequence = _lastRecvSequence;
    }
}

void SoapyStreamEndpoint::recvACK(void)
{
    StreamDatagramHeader header;
    int ret = _streamSock.recv(&header, sizeof(header));
    if (ret < 0)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "StreamEndpoint::recvACK(), FAILED %s", _streamSock.lastErrorMsg());
    }

    //check the header
    size_t bytes = ntohl(header.bytes);
    if (bytes > size_t(ret))
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "StreamEndpoint::recvACK(%d bytes), FAILED %d", int(bytes), ret);
    }

    _lastRecvSequence = ntohl(header.sequence);
}

/***********************************************************************
 * receive endpoint implementation
 **********************************************************************/
bool SoapyStreamEndpoint::waitRecv(const long timeoutUs)
{
    return _streamSock.selectRecv(timeoutUs);
}

int SoapyStreamEndpoint::acquireRecv(size_t &handle, const void **buffs, int &flags, long long &timeNs)
{
    //no available handles, the user is hoarding them...
    if (_numHandlesAcquired == _buffData.size())
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "StreamEndpoint::acquireRecv() -- all buffers acquired");
        return SOAPY_SDR_STREAM_ERROR;
    }

    //grab the current handle
    handle = _nextHandleAcquire;
    auto &data = _buffData[handle];

    //receive into the buffer
    assert(not _streamSock.null());
    int ret = _streamSock.recv(data.buff.data(), data.buff.size());
    if (ret < 0)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "StreamEndpoint::acquireRecv(), FAILED %s", _streamSock.lastErrorMsg());
        return SOAPY_SDR_STREAM_ERROR;
    }

    //check the header
    auto header = (const StreamDatagramHeader*)data.buff.data();
    size_t bytes = ntohl(header->bytes);
    if (bytes > size_t(ret))
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "StreamEndpoint::acquireRecv(%d bytes), FAILED %d\n"
            "This MTU setting may be unachievable. Check network configuration.", int(bytes), ret);
        return SOAPY_SDR_STREAM_ERROR;
    }
    const int numElemsOrErr = int(ntohl(header->elems));

    //TODO sequence out of order or skip failures

    //update flow control
    _lastRecvSequence = ntohl(header->sequence);
    this->sendACK();

    //increment for next handle
    if (numElemsOrErr >= 0)
    {
        data.acquired = true;
        _nextHandleAcquire = (_nextHandleAcquire + 1)%_numBuffs;
        _numHandlesAcquired++;
    }

    //set output parameters
    this->getAddrs(handle, (void **)buffs);
    flags = ntohl(header->flags);
    timeNs = ntohll(header->time);
    return numElemsOrErr;
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

/***********************************************************************
 * send endpoint implementation
 **********************************************************************/
bool SoapyStreamEndpoint::waitSend(const long timeoutUs)
{
    //are we within the allowed number of sequences in flight?
    while (uint32_t(_lastRecvSequence+_maxInFlightSeqs) < uint32_t(_lastSendSequence))
    {
        //wait for a flow control ACK to arrive
        if (not _streamSock.selectRecv(timeoutUs)) return false;

        //exhaustive receive without timeout
        while (_streamSock.selectRecv(0)) this->recvACK();
    }

    return true;
}

int SoapyStreamEndpoint::acquireSend(size_t &handle, void **buffs)
{
    //no available handles, the user is hoarding them...
    if (_numHandlesAcquired == _buffData.size())
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "StreamEndpoint::acquireSend() -- all buffers acquired");
        return SOAPY_SDR_STREAM_ERROR;
    }

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
    header->sequence = htonl(_lastSendSequence++);
    header->elems = htonl(numElemsOrErr);
    header->flags = htonl(flags);
    header->time = htonll(timeNs);

    //send from the buffer
    assert(not _streamSock.null());
    int ret = _streamSock.send(data.buff.data(), bytes);
    if (ret < 0)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "StreamEndpoint::releaseSend(), FAILED %s", _streamSock.lastErrorMsg());
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

/***********************************************************************
 * status endpoint implementation -- used by both directions
 **********************************************************************/
bool SoapyStreamEndpoint::waitStatus(const long timeoutUs)
{
    return _statusSock.selectRecv(timeoutUs);
}

int SoapyStreamEndpoint::readStatus(size_t &chanMask, int &flags, long long &timeNs)
{
    StreamDatagramHeader header;
    //read the status
    assert(not _statusSock.null());
    int ret = _statusSock.recv(&header, sizeof(header));
    if (ret < 0) return SOAPY_SDR_STREAM_ERROR;

    //check the header
    size_t bytes = ntohl(header.bytes);
    if (bytes > size_t(ret))
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "StreamEndpoint::readStatus(%d bytes), FAILED %d", int(bytes), ret);
        return SOAPY_SDR_STREAM_ERROR;
    }

    //set output parameters
    chanMask = ntohl(header.sequence);
    flags = ntohl(header.flags);
    timeNs = ntohll(header.time);
    return int(ntohl(header.elems));
}

void SoapyStreamEndpoint::writeStatus(const int code, const size_t chanMask, const int flags, const long long timeNs)
{
    StreamDatagramHeader header;
    header.bytes = htonl(sizeof(header));
    header.sequence = htonl(chanMask);
    header.flags = htonl(flags);
    header.time = htonll(timeNs);
    header.elems = htonl(code);

    //send the status
    assert(not _statusSock.null());
    int ret = _statusSock.send(&header, sizeof(header));
    if (ret < 0)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "StreamEndpoint::writeStatus(), FAILED %s", _statusSock.lastErrorMsg());
    }
    else if (size_t(ret) != sizeof(header))
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "StreamEndpoint::writeStatus(%d bytes), FAILED %d", int(sizeof(header)), ret);
    }
}
