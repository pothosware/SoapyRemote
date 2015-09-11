// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyClient.hpp"
#include "SoapyRPCPacker.hpp"
#include "SoapyRPCUnpacker.hpp"
#include "SoapySocketSession.hpp"
#include <SoapySDR/Registry.hpp>
#include <SoapySDR/Logger.hpp>

/***********************************************************************
 * Discovery routine -- connect to server when key specified
 **********************************************************************/
static std::vector<SoapySDR::Kwargs> findRemote(const SoapySDR::Kwargs &args)
{
    std::vector<SoapySDR::Kwargs> result;

    if (args.count(SOAPY_REMOTE_KWARG_STOP) != 0) return result;
    if (args.count(SOAPY_REMOTE_KWARG_KEY) == 0) return result;
    const std::string url = args.at(SOAPY_REMOTE_KWARG_KEY);

    //try to connect to the remote server
    SoapySocketSession sess;
    SoapyRPCSocket s;
    int ret = s.connect(url);
    if (ret != 0)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "SoapyRemote::find() -- connect FAIL: %s", s.lastErrorMsg());
        return result;
    }

    //send the args
    try
    {
        SoapyRPCPacker packer(s);
        packer & SOAPY_REMOTE_FIND;
        packer & args;
        packer();
    }
    catch (const std::exception &ex)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "SoapyRemote::find() -- pack FAIL: %s", ex.what());
    }

    //receive the result
    try
    {
        SoapyRPCUnpacker unpacker(s);
        unpacker & result;
    }
    catch (const std::exception &ex)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "SoapyRemote::find() -- unpack FAIL: %s", ex.what());
    }

    return result;
}

/***********************************************************************
 * Factory routine -- connect to server and create remote device
 **********************************************************************/
static SoapySDR::Device *makeRemote(const SoapySDR::Kwargs &args)
{
    SoapySocketSession sess;
    return NULL; //TODO
}

/***********************************************************************
 * Registration
 **********************************************************************/
static SoapySDR::Registry registerRemote("remote", &findRemote, &makeRemote, SOAPY_SDR_ABI_VERSION);
