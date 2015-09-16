// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapySendEndpoint.hpp"
#include "SoapyRPCSocket.hpp"
#include "SoapyRemoteDefs.hpp"

SoapySendEndpoint::SoapySendEndpoint(
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

SoapySendEndpoint::~SoapySendEndpoint(void)
{
    
}

void SoapySendEndpoint::getAddrs(const size_t handle, void **buffs) const
{
    
}

bool SoapySendEndpoint::wait(const long timeoutUs)
{
    
}

int SoapySendEndpoint::acquire(size_t &handle, void **buffs)
{
    
}

void SoapySendEndpoint::release(const size_t handle, const int numElemsOrErr, int &flags, const long long timeNs)
{
    
}
