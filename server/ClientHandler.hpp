// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <cstddef>
#include <map>

class SoapyRPCSocket;
class SoapyRPCPacker;
class SoapyRPCUnpacker;
class SoapyLogForwarder;
class ServerStreamData;

namespace SoapySDR
{
    class Device;
}

/*!
 * The client handler manages a remote client.
 */
class SoapyClientHandler
{
public:
    SoapyClientHandler(SoapyRPCSocket &sock);

    ~SoapyClientHandler(void);

    //handle once, return true to call again
    bool handleOnce(void);

private:
    bool handleOnce(SoapyRPCUnpacker &unpacker, SoapyRPCPacker &packer);

    SoapyRPCSocket &_sock;
    SoapySDR::Device *_dev;
    SoapyLogForwarder *_logForwarder;

    //stream tracking
    int _nextStreamId;
    std::map<int, ServerStreamData> _streamData;
};
