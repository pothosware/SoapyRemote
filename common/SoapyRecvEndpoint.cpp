// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyRecvEndpoint.hpp"
#include "SoapyRPCSocket.hpp"
#include "SoapyRemoteDefs.hpp"

SoapyRecvEndpoint::SoapyRecvEndpoint(
    SoapyRPCSocket &sock,
    const size_t numChans,
    const size_t elemSize,
    const size_t mtu,
    const size_t window):
    _sock(sock),
    _numChans(numChans),
    _elemSize(elemSize),
    _buffSize(mtu/numChans/elemSize),
    _numBuffs(SOAPY_REMOTE_ENDPOINT_NUM_BUFFS)
{
    
}

SoapyRecvEndpoint::~SoapyRecvEndpoint(void)
{
    
}

bool SoapyRecvEndpoint::wait(const long timeoutUs)
{
    return _sock.selectRecv(timeoutUs);
}

int SoapyRecvEndpoint::acquire(size_t &handle, const void **buffs, int &flags, long long &timeNs)
{
    
}

void SoapyRecvEndpoint::release(const size_t handle)
{
    
}
