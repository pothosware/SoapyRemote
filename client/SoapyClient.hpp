// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyRemoteCommon.hpp"
#include "SoapyRPCSocket.hpp"
#include "SoapySocketSession.hpp"
#include <SoapySDR/Device.hpp>

class SoapyRemoteDevice : public SoapySDR::Device
{
public:
    SoapyRemoteDevice(const SoapySDR::Kwargs &args);

    ~SoapyRemoteDevice(void);

private:
    SoapySocketSession _sess;
    SoapyRPCSocket _sock;
};
