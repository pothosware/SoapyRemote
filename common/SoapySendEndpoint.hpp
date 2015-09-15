// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include "SoapyRPCSocket.hpp"

/*!
 * The send endpoint supports datagram transmission over a windowed link.
 */
class SoapySendEndpoint
{
public:
    SoapySendEndpoint(
        SoapyRPCSocket &sock,
        const size_t numChans,
        const size_t buffSize,
        const size_t numBuffs);

    ~SoapySendEndpoint(void);

    //! Actual buffer size in bytes
    size_t getBuffSize(void) const;

    //! Actual number of buffers
    size_t getNumBuffs(void) const;

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
     */
    void release(const size_t handle, const size_t numElems, int &flags, const long long timeNs);

private:
    SoapyRPCSocket &_sock;
};
