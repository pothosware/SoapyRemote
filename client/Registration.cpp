// Copyright (c) 2015-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyClient.hpp"
#include "LogAcceptor.hpp"
#include "SoapyURLUtils.hpp"
#include "SoapyRemoteDefs.hpp"
#include "SoapyRPCPacker.hpp"
#include "SoapyRPCUnpacker.hpp"
#include <SoapySDR/Registry.hpp>
#include <SoapySDR/Logger.hpp>
#include <future>

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

    //extract timeout
    long timeoutUs = SOAPY_REMOTE_SOCKET_TIMEOUT_US;
    const auto timeoutIt = args.find("remote:timeout");
    if (timeoutIt != args.end()) timeoutUs = std::stol(timeoutIt->second);

    //no remote specified, use the discovery protocol
    if (args.count("remote") == 0)
    {
        //determine IP version preferences
        int ipVer(4);
        const auto ipVerIt = args.find("remote:ipver");
        if (ipVerIt != args.end()) ipVer = std::stoi(ipVerIt->second);

        //spawn futures to connect to each remote
        std::vector<std::future<SoapySDR::KwargsList>> futures;
        for (const auto &url : SoapyRemoteDevice::getServerURLs(ipVer, timeoutUs))
        {
            auto argsWithURL = args;
            argsWithURL["remote"] = url;
            futures.push_back(std::async(std::launch::async, &findRemote, argsWithURL));
        }

        //wait on all futures for results
        for (auto &future : futures)
        {
            const auto subResult = future.get();
            result.insert(result.end(), subResult.begin(), subResult.end());
        }

        return result;
    }

    //otherwise connect to a specific url and enumerate
    auto url = SoapyURL(args.at("remote"));
    SoapySDR::logf(SOAPY_SDR_DEBUG, "SoapyClient querying devices for %s", url.toString().c_str());

    //default url parameters when not specified
    if (url.getScheme().empty()) url.setScheme("tcp");
    if (url.getService().empty()) url.setService(SOAPY_REMOTE_DEFAULT_SERVICE);

    //The first connection may be delayed by ARP, either on the client
    //or the server side as previous communication was multi-casted.
    //To be consistent with the normal specified timeout value,
    //the first connection timeout is increased to compensate.
    const long arpTimeout(SOAPY_REMOTE_SOCKET_TIMEOUT_US);

    //try to connect to the remote server
    SoapySocketSession sess;
    SoapyRPCSocket s;
    int ret = s.connect(url.toString(), timeoutUs+arpTimeout);
    if (ret != 0)
    {
        SoapySDR::logf(SOAPY_SDR_DEBUG, "SoapyRemote::find() -- connect(%s) FAIL: %s", url.toString().c_str(), s.lastErrorMsg());
        return result;
    }

    //find transaction
    try
    {
        //No log forwarding during discovery unless debug build:
        #ifndef NDEBUG
        SoapyLogAcceptor logAcceptor(url.toString(), s, timeoutUs);
        #endif //NDEBUG

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
        SoapySDR::logf(SOAPY_SDR_ERROR, "SoapyRemote::find(%s) -- transact FAIL: %s", url.toString().c_str(), ex.what());
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
