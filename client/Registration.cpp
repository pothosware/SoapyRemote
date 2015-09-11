// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyClient.hpp"
#include "SoapyRPCPacker.hpp"
#include <SoapySDR/Registry.hpp>

static std::vector<SoapySDR::Kwargs> findRemote(const SoapySDR::Kwargs &args)
{
    if (args.count(SOAPY_REMOTE_KWARG_KEY) == 0) return std::vector<SoapySDR::Kwargs>();
    const std::string url = args.at(SOAPY_REMOTE_KWARG_KEY);

    SoapyRPCSocket s;
    if (s.connect(url) != 0) return std::vector<SoapySDR::Kwargs>();

    //TODO
    SoapyRPCPacker packer(s);
    packer & args;

    return std::vector<SoapySDR::Kwargs>();
}

static SoapySDR::Device *makeRemote(const SoapySDR::Kwargs &args)
{
    return NULL; //TODO
}

static SoapySDR::Registry registerRemote("remote", &findRemote, &makeRemote, SOAPY_SDR_ABI_VERSION);
