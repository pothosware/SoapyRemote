// Copyright (c) 2015-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include "SoapyRemoteConfig.hpp"
#include <cstddef>
#include <vector>

class SoapyRPCSocket;

/*!
 * The stream endpoint supports a windowed link datagram protocol.
 * This endpoint can be operated in only one mode: receive or send,
 * and must be paired with another differently configured endpoint.
 */
class SOAPY_REMOTE_API SoapyStreamEndpoint
{
public:
    SoapyStreamEndpoint(
        SoapyRPCSocket &streamSock,
        SoapyRPCSocket &statusSock,
        const bool datagramMode,
        const bool isRecv,
        const size_t numChans,
        const size_t elemSize,
        const size_t mtu,
        const size_t window);

    ~SoapyStreamEndpoint(void);

    //! How many channels configured
    size_t getNumChans(void) const
    {
        return _numChans;
    }

    //! Element size in bytes
    size_t getElemSize(void) const
    {
        return _elemSize;
    }

    //! Actual buffer size in elements
    size_t getBuffSize(void) const
    {
        return _buffSize;
    }

    //! Actual number of buffers
    size_t getNumBuffs(void) const
    {
        return _numBuffs;
    }

    //! Query handle addresses
    void getAddrs(const size_t handle, void **buffs) const
    {
        for (size_t i = 0; i < _numChans; i++)
        {
            buffs[i] = _buffData[handle].buffs[i];
        }
    }

    /*******************************************************************
     * receive endpoint API
     ******************************************************************/

    /*!
     * Wait for a datagram to arrive at the socket
     * return true when ready for false for timeout.
     */
    bool waitRecv(const long timeoutUs);

    /*!
     * Acquire a receive buffer with metadata.
     * return the number of elements or error code
     */
    int acquireRecv(size_t &handle, const void **buffs, int &flags, long long &timeNs);

    /*!
     * Release the buffer when done.
     */
    void releaseRecv(const size_t handle);

    /*******************************************************************
     * send endpoint API
     ******************************************************************/

    /*!
     * Wait for the flow control to allow transmission.
     * return true when ready for false for timeout.
     */
    bool waitSend(const long timeoutUs);

    /*!
     * Acquire a receive buffer with metadata.
     */
    int acquireSend(size_t &handle, void **buffs);

    /*!
     * Release the buffer when done.
     * pass in the number of elements or error code
     */
    void releaseSend(const size_t handle, const int numElemsOrErr, int &flags, const long long timeNs);

    /*******************************************************************
     * status endpoint API -- used by both directions
     ******************************************************************/

    /*!
     * Wait for a status message to arrive
     */
    bool waitStatus(const long timeoutUs);

    /*!
     * Read the stream status data.
     * Return 0 or error code.
     */
    int readStatus(size_t &chanMask, int &flags, long long &timeNs);

    /*!
     * Write the stream status from the forwarder.
     */
    void writeStatus(const int code, const size_t chanMask, const int flags, const long long timeNs);

private:
    SoapyRPCSocket &_streamSock;
    SoapyRPCSocket &_statusSock;
    const bool _datagramMode;
    const size_t _xferSize;
    const size_t _numChans;
    const size_t _elemSize;
    const size_t _buffSize;
    const size_t _numBuffs;

    struct BufferData
    {
        std::vector<char> buff; //actual POD
        std::vector<void *> buffs; //pointers
        bool acquired;
    };
    std::vector<BufferData> _buffData;

    //acquire+release tracking
    size_t _nextHandleAcquire;
    size_t _nextHandleRelease;
    size_t _numHandlesAcquired;

    //sequence tracking
    size_t _lastSendSequence;
    size_t _lastRecvSequence;
    size_t _maxInFlightSeqs;
    bool _receiveInitial;

    //how often to send a flow control ACK? (recv only)
    size_t _triggerAckWindow;

    //flow control helpers
    void sendACK(void);
    void recvACK(void);
};
