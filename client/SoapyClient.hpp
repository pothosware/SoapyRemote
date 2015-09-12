// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyRemoteCommon.hpp"
#include "SoapyRPCSocket.hpp"
#include "SoapySocketSession.hpp"
#include <SoapySDR/Device.hpp>
#include <mutex>

class SoapyRemoteDevice : public SoapySDR::Device
{
public:
    SoapyRemoteDevice(const SoapySDR::Kwargs &args);

    ~SoapyRemoteDevice(void);

    /*******************************************************************
     * Identification API
     ******************************************************************/

    std::string getDriverKey(void) const;

    std::string getHardwareKey(void) const;

    SoapySDR::Kwargs getHardwareInfo(void) const;

    /*******************************************************************
     * Channels API
     ******************************************************************/

    void setFrontendMapping(const int direction, const std::string &mapping);

    std::string getFrontendMapping(const int direction) const;

    size_t getNumChannels(const int direction) const;

    bool getFullDuplex(const int direction, const size_t channel) const;

private:
    SoapySocketSession _sess;
    SoapyRPCSocket _sock;
    std::mutex _mutex;
};
