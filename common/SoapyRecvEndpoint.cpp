// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyRecvEndpoint.hpp"
#include "SoapyRPCSocket.hpp"

SoapyRecvEndpoint::SoapyRecvEndpoint(
    SoapyRPCSocket &sock,
    const size_t numChans,
    const size_t buffSize,
    const size_t numBuffs):
    _sock(sock)
{
    
}

SoapyRecvEndpoint::~SoapyRecvEndpoint(void)
{
    
}

size_t SoapyRecvEndpoint::getBuffSize(void) const
{
    
}

size_t SoapyRecvEndpoint::getNumBuffs(void) const
{
    
}

void SoapyRecvEndpoint::getAddrs(const size_t handle, void **buffs) const
{
    
}

bool SoapyRecvEndpoint::wait(const long timeoutUs)
{
    
}

int SoapyRecvEndpoint::acquire(size_t &handle, const void **buffs, int &flags, long long &timeNs)
{
    
}

void SoapyRecvEndpoint::release(const size_t handle)
{
    
}
