// Copyright (c) 2015-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <SoapySDR/Errors.hpp>
#include <SoapySDR/Logger.hpp>
#include "SoapyStreamEndpoint.hpp"
#include "SoapyRPCSocket.hpp"
#include "SoapyURLUtils.hpp"
#include "SoapyRemoteDefs.hpp"
#include "SoapySocketDefs.hpp"
#include <algorithm> //min/max
#include <cassert>
#include <cstdint>

#define HEADER_SIZE sizeof(StreamDatagramHeader)

//use the larger IPv6 header size
#define PROTO_HEADER_SIZE (40 + 8) //IPv6 + UDP

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
    const bool datagramMode,
    const bool isRecv,
    const size_t numChans,
    const size_t elemSize,
    const size_t mtu,
    const size_t window):
    _streamSock(streamSock),
    _statusSock(statusSock),
    _datagramMode(datagramMode),
    _xferSize(mtu-PROTO_HEADER_SIZE),
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
    _receiveInitial(false),
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

    //endpoints require a large socket buffer in the data direction
    int ret = _streamSock.setBuffSize(isRecv, window);
    if (ret != 0)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "StreamEndpoint resize socket buffer to %d KiB failed\n  %s", int(window/1024), _streamSock.lastErrorMsg());
    }

    //log when the size is not expected, users may have to tweak system parameters
    int actualWindow = _streamSock.getBuffSize(isRecv);
    if (actualWindow < 0)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "StreamEndpoint get socket buffer size failed\n  %s", _streamSock.lastErrorMsg());
        actualWindow = window;
    }
    else if (size_t(actualWindow) < window)
    {
        SoapySDR::logf(SOAPY_SDR_WARNING, "StreamEndpoint resize socket buffer: set %d KiB, got %d KiB", int(window/1024), int(actualWindow/1024));
    }

    //print summary
    SoapySDR::logf(SOAPY_SDR_INFO, "Configured %s endpoint: dgram=%d bytes, %d elements @ %d bytes, window=%d KiB",
        isRecv?"receiver":"sender", int(_xferSize), int(_buffSize*_numChans), int(_elemSize), int(actualWindow/1024));

    //calculate flow control window
    if (isRecv)
    {
        //calculate maximum in-flight sequences allowed
        _maxInFlightSeqs = actualWindow/mtu;

        //calculate the flow control ACK conditions
        _triggerAckWindow = _maxInFlightSeqs/_numBuffs;

        //send gratuitous ack to set sender's window
        this->sendACK();
    }
    else
    {
        //_maxInFlightSeqs set by flow control packet
    }
}

SoapyStreamEndpoint::~SoapyStreamEndpoint(void)
{
    return;
}

void SoapyStreamEndpoint::sendACK(void)
{
    StreamDatagramHeader header;
    header.bytes = htonl(sizeof(header));
    header.sequence = htonl(_lastRecvSequence);
    header.elems = htonl(_maxInFlightSeqs);
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

void SoapyStreamEndpoint::recvACK(void)
{
    StreamDatagramHeader header;
    int ret = _streamSock.recv(&header, sizeof(header));
    if (ret < 0)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "StreamEndpoint::recvACK(), FAILED %s", _streamSock.lastErrorMsg());
    }
    _receiveInitial = true;

    //check the header
    size_t bytes = ntohl(header.bytes);
    if (bytes > size_t(ret))
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "StreamEndpoint::recvACK(%d bytes), FAILED %d", int(bytes), ret);
    }

    _lastRecvSequence = ntohl(header.sequence);
    _maxInFlightSeqs = ntohl(header.elems);
}

/***********************************************************************
 * receive endpoint implementation
 **********************************************************************/
bool SoapyStreamEndpoint::waitRecv(const long timeoutUs)
{
    //send gratuitous ack until something is received
    if (not _receiveInitial) this->sendACK();
    return _streamSock.selectRecv(timeoutUs);
}

int SoapyStreamEndpoint::acquireRecv(size_t &handle, const void **buffs, int &flags, long long &timeNs)
{
    int ret = 0;

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
    if (_datagramMode) ret = _streamSock.recv(data.buff.data(), data.buff.size());
    else ret = _streamSock.recv(data.buff.data(), HEADER_SIZE, MSG_WAITALL);
    if (ret < 0)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "StreamEndpoint::acquireRecv(), FAILED %s", _streamSock.lastErrorMsg());
        return SOAPY_SDR_STREAM_ERROR;
    }
    size_t bytesRecvd = size_t(ret);
    _receiveInitial = true;

    //check the header
    auto header = (const StreamDatagramHeader*)data.buff.data();
    size_t bytes = ntohl(header->bytes);

    if (_datagramMode and bytes > bytesRecvd)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "StreamEndpoint::acquireRecv(%d bytes), FAILED %d\n"
            "This MTU setting may be unachievable. Check network configuration.", int(bytes), ret);
        return SOAPY_SDR_STREAM_ERROR;
    }

    else while (bytesRecvd < bytes)
    {
        ret = _streamSock.recv(data.buff.data()+bytesRecvd, std::min<size_t>(SOAPY_REMOTE_SOCKET_BUFFMAX, bytes-bytesRecvd));
        if (ret < 0)
        {
            SoapySDR::logf(SOAPY_SDR_ERROR, "StreamEndpoint::acquireRecv(), FAILED %s", _streamSock.lastErrorMsg());
            return SOAPY_SDR_STREAM_ERROR;
        }
        bytesRecvd += size_t(ret);
    }

    const int numElemsOrErr = int(ntohl(header->elems));

    //dropped or out of order packets
    //TODO return an error code, more than a notification
    if (uint32_t(_lastRecvSequence) != uint32_t(ntohl(header->sequence)))
    {
        SoapySDR::log(SOAPY_SDR_SSI, "S");
    }

    //update flow control
    _lastRecvSequence = ntohl(header->sequence)+1;

    //has there been at least trigger window number of sequences since the last ACK?
    if (uint32_t(_lastRecvSequence-_lastSendSequence) >= _triggerAckWindow)
    {
        this->sendACK();
    }

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
    while (not _receiveInitial or uint32_t(_lastSendSequence-_lastRecvSequence) >= _maxInFlightSeqs)
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

    //The first N-1 channels must be complete buffSize sends
    //due to the pointer allocation at initialization time.
    //The last channel can be shortened to the available numElems.
    const size_t totalElems = ((_numChans-1)*_buffSize) + numElemsOrErr;

    //load the header
    auto header = (StreamDatagramHeader*)data.buff.data();
    size_t bytes = HEADER_SIZE + ((numElemsOrErr < 0)?0:(totalElems*_elemSize));
    header->bytes = htonl(bytes);
    header->sequence = htonl(_lastSendSequence++);
    header->elems = htonl(numElemsOrErr);
    header->flags = htonl(flags);
    header->time = htonll(timeNs);

    //send from the buffer
    assert(not _streamSock.null());
    size_t bytesSent = 0;
    while (bytesSent < bytes)
    {
        int ret = _streamSock.send(data.buff.data()+bytesSent, std::min<size_t>(SOAPY_REMOTE_SOCKET_BUFFMAX, bytes-bytesSent));
        if (ret < 0)
        {
            SoapySDR::logf(SOAPY_SDR_ERROR, "StreamEndpoint::releaseSend(), FAILED %s", _streamSock.lastErrorMsg());
            break;
        }
        bytesSent += size_t(ret);
        if (not _datagramMode) continue;
        if (bytesSent != bytes)
        {
            SoapySDR::logf(SOAPY_SDR_ERROR, "StreamEndpoint::releaseSend(%d bytes), FAILED %d", int(bytes), ret);
        }
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
