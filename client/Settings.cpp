// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyClient.hpp"
#include "SoapyRPCPacker.hpp"
#include "SoapyRPCUnpacker.hpp"
#include "SoapySocketSession.hpp"
#include <SoapySDR/Logger.hpp>
#include <stdexcept>

//lazy fix for the const call issue -- FIXME
#define _mutex const_cast<std::mutex &>(_mutex)
#define _sock const_cast<SoapyRPCSocket &>(_sock)

SoapyRemoteDevice::SoapyRemoteDevice(const SoapySDR::Kwargs &args)
{
    if (args.count(SOAPY_REMOTE_KWARG_STOP) != 0) //probably wont happen
    {
        throw std::runtime_error("SoapyRemoteDevice() -- factory loop");
    }

    if (args.count(SOAPY_REMOTE_KWARG_KEY) == 0)
    {
        throw std::runtime_error("SoapyRemoteDevice() -- missing URL");
    }

    const std::string url = args.at(SOAPY_REMOTE_KWARG_KEY);

    //try to connect to the remote server
    int ret = _sock.connect(url);
    if (ret != 0)
    {
        throw std::runtime_error("SoapyRemoteDevice("+url+") -- connect FAIL: " + _sock.lastErrorMsg());
    }

    //send the args
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_MAKE;
    packer & args;
    packer();

    //receive the result
    SoapyRPCUnpacker unpacker(_sock);
}

SoapyRemoteDevice::~SoapyRemoteDevice(void)
{
    //cant throw in the destructor
    try
    {
        //send the args
        SoapyRPCPacker packer(_sock);
        packer & SOAPY_REMOTE_UNMAKE;
        packer();

        //receive the result
        SoapyRPCUnpacker unpacker(_sock);
    }
    catch (const std::exception &ex)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "~SoapyRemoteDevice() FAIL: %s", ex.what());
    }
}

/*******************************************************************
 * Identification API
 ******************************************************************/

std::string SoapyRemoteDevice::getDriverKey(void) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_DRIVER_KEY;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    std::string result;
    unpacker & result;
    return result;
}

std::string SoapyRemoteDevice::getHardwareKey(void) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_HARDWARE_KEY;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    std::string result;
    unpacker & result;
    return result;
}

SoapySDR::Kwargs SoapyRemoteDevice::getHardwareInfo(void) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_HARDWARE_INFO;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    SoapySDR::Kwargs result;
    unpacker & result;
    return result;
}

/*******************************************************************
 * Channels API
 ******************************************************************/

void SoapyRemoteDevice::setFrontendMapping(const int direction, const std::string &mapping)
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_SET_FRONTEND_MAPPING;
    packer & char(direction);
    packer & mapping;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
}

std::string SoapyRemoteDevice::getFrontendMapping(const int direction) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_FRONTEND_MAPPING;
    packer & char(direction);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    std::string result;
    unpacker & result;
    return result;
}

size_t SoapyRemoteDevice::getNumChannels(const int direction) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_NUM_CHANNELS;
    packer & char(direction);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    int result;
    unpacker & result;
    return result;
}

bool SoapyRemoteDevice::getFullDuplex(const int direction, const size_t channel) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_FULL_DUPLEX;
    packer & char(direction);
    packer & int(channel);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    bool result;
    unpacker & result;
    return result;
}
