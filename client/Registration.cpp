// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyClient.hpp"
#include "LogAcceptor.hpp"
#include "SoapyRemoteDefs.hpp"
#include "SoapyRPCPacker.hpp"
#include "SoapyRPCUnpacker.hpp"
#include <SoapySDR/Registry.hpp>
#include <SoapySDR/Logger.hpp>

/***********************************************************************
 * Args translator for nested keywords
 **********************************************************************/
static SoapySDR::Kwargs translateArgs(const SoapySDR::Kwargs &args)
{
    SoapySDR::Kwargs argsOut;

    //stop infinite loops with special keyword
    argsOut[SOAPY_REMOTE_KWARG_STOP] = "";

    //copy all non-remote keys
    for (auto &pair : args)
    {
        if (pair.first == "driver") continue; //don't propagate local driver filter
        if (pair.first == "type") continue; //don't propagate local sub-type filter
        if (pair.first.find(SOAPY_REMOTE_KWARG_PREFIX) == std::string::npos)
        {
            argsOut[pair.first] = pair.second;
        }
    }

    //write all remote keys with prefix stripped
    for (auto &pair : args)
    {
        if (pair.first.find(SOAPY_REMOTE_KWARG_PREFIX) == 0)
        {
            static const size_t offset = std::string(SOAPY_REMOTE_KWARG_PREFIX).size();
            argsOut[pair.first.substr(offset)] = pair.second;
        }
    }

    return argsOut;
}

/***********************************************************************
 * Discovery routine -- connect to server when key specified
 **********************************************************************/
static std::vector<SoapySDR::Kwargs> findRemote(const SoapySDR::Kwargs &args)
{
    std::vector<SoapySDR::Kwargs> result;

    if (args.count(SOAPY_REMOTE_KWARG_STOP) != 0) return result;
    if (args.count("remote") == 0) return result;
    const std::string url = args.at("remote");

    //try to connect to the remote server
    SoapySocketSession sess;
    SoapyRPCSocket s;
    int ret = s.connect(url);
    if (ret != 0)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "SoapyRemote::find() -- connect FAIL: %s", s.lastErrorMsg());
        return result;
    }

    //find transaction
    try
    {
        SoapyLogAcceptor logAcceptor(url, s);

        SoapyRPCPacker packer(s);
        packer & SOAPY_REMOTE_FIND;
        packer & translateArgs(args);
        packer();
        SoapyRPCUnpacker unpacker(s);
        unpacker & result;

        //graceful disconnect
        SoapyRPCPacker packerHangup(s);
        packerHangup & SOAPY_REMOTE_HANGUP;
        packerHangup();
        SoapyRPCUnpacker unpackerHangup(s);
    }
    catch (const std::exception &ex)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "SoapyRemote::find() -- transact FAIL: %s", ex.what());
    }

    return result;
}

/***********************************************************************
 * Factory routine -- connect to server and create remote device
 **********************************************************************/
static SoapySDR::Device *makeRemote(const SoapySDR::Kwargs &args)
{
    if (args.count(SOAPY_REMOTE_KWARG_STOP) != 0) //probably wont happen
    {
        throw std::runtime_error("SoapyRemoteDevice() -- factory loop");
    }

    if (args.count("remote") == 0)
    {
        throw std::runtime_error("SoapyRemoteDevice() -- missing URL");
    }

    const std::string url = args.at("remote");

    return new SoapyRemoteDevice(url, translateArgs(args));
}

/***********************************************************************
 * Registration
 **********************************************************************/
static SoapySDR::Registry registerRemote("remote", &findRemote, &makeRemote, SOAPY_SDR_ABI_VERSION);
