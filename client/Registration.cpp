// Copyright (c) 2015-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyClient.hpp"
#include "LogAcceptor.hpp"
#include "SoapySSDPEndpoint.hpp"
#include "SoapyURLUtils.hpp"
#include "SoapyRemoteDefs.hpp"
#include "SoapyRPCPacker.hpp"
#include "SoapyRPCUnpacker.hpp"
#include <SoapySDR/Registry.hpp>
#include <SoapySDR/Logger.hpp>
#include <thread>

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

    //no remote specified, use the discovery protocol
    if (args.count("remote") == 0)
    {
        //On non-windows platforms the endpoint instance can last the
        //duration of the process because it can be cleaned up safely.
        //Windows has issues cleaning up threads and sockets on exit.
        #ifndef _MSC_VER
        static
        #endif //_MSC_VER
        auto ssdpEndpoint = SoapySSDPEndpoint::getInstance();

        //enable forces new search queries
        ssdpEndpoint->enablePeriodicSearch(true);

        //wait maximum timeout for replies
        std::this_thread::sleep_for(std::chrono::microseconds(SOAPY_REMOTE_SOCKET_TIMEOUT_US));

        //determine IP version preferences
        int ipVer(4);
        const auto ipVerIt = args.find("remote:ipver");
        if (ipVerIt != args.end()) ipVer = std::stoi(ipVerIt->second);

        for (const auto &url : SoapySSDPEndpoint::getInstance()->getServerURLs(ipVer))
        {
            auto argsWithURL = args;
            argsWithURL["remote"] = url;
            const auto subResult = findRemote(argsWithURL);
            result.insert(result.end(), subResult.begin(), subResult.end());
        }

        return result;
    }

    //otherwise connect to a specific url and enumerate
    auto url = SoapyURL(args.at("remote"));

    //default url parameters when not specified
    if (url.getScheme().empty()) url.setScheme("tcp");
    if (url.getService().empty()) url.setService(SOAPY_REMOTE_DEFAULT_SERVICE);

    //try to connect to the remote server
    SoapySocketSession sess;
    SoapyRPCSocket s;
    int ret = s.connect(url.toString());
    if (ret != 0)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "SoapyRemote::find() -- connect(%s) FAIL: %s", url.toString().c_str(), s.lastErrorMsg());
        return result;
    }

    //find transaction
    try
    {
        SoapyLogAcceptor logAcceptor(url.toString(), s);

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

    //remove instances of the stop key from the result
    for (auto &resultArgs : result)
    {
        resultArgs.erase(SOAPY_REMOTE_KWARG_STOP);
        if (resultArgs.count("driver") != 0)
        {
            resultArgs["remote:driver"] = resultArgs.at("driver");
            resultArgs.erase("driver");
        }
        if (resultArgs.count("type") != 0)
        {
            resultArgs["remote:type"] = resultArgs.at("type");
            resultArgs.erase("type");
        }
        resultArgs["remote"] = url.toString();
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

    auto url = SoapyURL(args.at("remote"));

    //default url parameters when not specified
    if (url.getScheme().empty()) url.setScheme("tcp");
    if (url.getService().empty()) url.setService(SOAPY_REMOTE_DEFAULT_SERVICE);

    return new SoapyRemoteDevice(url.toString(), translateArgs(args));
}

/***********************************************************************
 * Registration
 **********************************************************************/
static SoapySDR::Registry registerRemote("remote", &findRemote, &makeRemote, SOAPY_SDR_ABI_VERSION);
