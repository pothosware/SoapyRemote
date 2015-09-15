// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapySendEndpoint.hpp"

SoapySendEndpoint::SoapySendEndpoint(
    SoapyRPCSocket &sock,
    const size_t numChans,
    const size_t buffSize,
    const size_t numBuffs):
    _sock(sock)
{
    
}

SoapySendEndpoint::~SoapySendEndpoint(void)
{
    
}

size_t SoapySendEndpoint::getBuffSize(void) const
{
    
}

size_t SoapySendEndpoint::getNumBuffs(void) const
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

void SoapySendEndpoint::release(const size_t handle, const size_t numElems, int &flags, const long long timeNs)
{
    
}
