// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include "SoapyRemoteConfig.hpp"
#include <cstddef>

class SoapyRPCSocket;

/*!
 * The send endpoint supports datagram transmission over a windowed link.
 */
class SOAPY_REMOTE_API SoapySendEndpoint
{
public:
    SoapySendEndpoint(
        SoapyRPCSocket &sock,
        const size_t numChans,
        const size_t elemSize,
        const size_t mtu,
        const size_t window);

    ~SoapySendEndpoint(void);

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
    void getAddrs(const size_t handle, void **buffs) const;

    /*!
     * Wait for the flow control to allow transmission.
     * return true when ready for false for timeout.
     */
    bool wait(const long timeoutUs);

    /*!
     * Acquire a receive buffer with metadata.
     */
    int acquire(size_t &handle, void **buffs);

    /*!
     * Release the buffer when done.
     * pass in the number of elements or error code
     */
    void release(const size_t handle, const int numElemsOrErr, int &flags, const long long timeNs);

private:
    SoapyRPCSocket &_sock;
    const size_t _numChans;
    const size_t _elemSize;;
    const size_t _buffSize;
    const size_t _numBuffs;
};
