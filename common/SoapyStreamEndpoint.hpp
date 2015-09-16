// Copyright (c) 2015-2015 Josh Blum
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
        SoapyRPCSocket &sock,
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

private:
    SoapyRPCSocket &_sock;
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

    size_t _nextHandleAcquire;
    size_t _nextHandleRelease;
    size_t _numHandlesAcquired;
    size_t _nextSequenceNumber;
};
