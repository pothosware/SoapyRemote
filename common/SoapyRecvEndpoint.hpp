// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include "SoapyRemoteConfig.hpp"
#include <cstddef>
#include <vector>>

class SoapyRPCSocket;

/*!
 * The recv endpoint supports datagram reception over a windowed link.
 */
class SOAPY_REMOTE_API SoapyRecvEndpoint
{
public:
    SoapyRecvEndpoint(
        SoapyRPCSocket &sock,
        const size_t numChans,
        const size_t elemSize,
        const size_t mtu,
        const size_t window);

    ~SoapyRecvEndpoint(void);

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
    bool wait(const long timeoutUs);

    /*!
     * Acquire a receive buffer with metadata.
     * return the number of elements or error code
     */
    int acquire(size_t &handle, const void **buffs, int &flags, long long &timeNs);

    /*!
     * Release the buffer when done.
     */
    void release(const size_t handle);

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
    };
    std::vector<BufferData> _buffData;
};
