// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyClient.hpp"
#include "SoapyRPCPacker.hpp"
#include "SoapyRPCUnpacker.hpp"
#include "SoapySocketSession.hpp"
#include <SoapySDR/Logger.hpp>
#include <stdexcept>

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
