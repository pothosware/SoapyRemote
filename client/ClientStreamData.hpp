// Copyright (c) 2015-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include "SoapyRPCSocket.hpp"
#include <vector>
#include <string>

class SoapyStreamEndpoint;

enum ConvertTypes
{
    CONVERT_MEMCPY,
    CONVERT_CF32_CS16,
    CONVERT_CF32_CS12,
    CONVERT_CS16_CS12,
    CONVERT_CS16_CS8,
    CONVERT_CF32_CS8,
    CONVERT_CF32_CU8,
};

struct ClientStreamData
{
    ClientStreamData(void);

    //string formats in use
    std::string localFormat;
    std::string remoteFormat;

    //this ID identifies the stream to the remote host
    int streamId;

    //datagram socket for stream endpoint
    SoapyRPCSocket streamSock;

    //datagram socket for status endpoint
    SoapyRPCSocket statusSock;

    //local side of the stream endpoint
    SoapyStreamEndpoint *endpoint;

    //buffer pointers to read/write API
    std::vector<const void *> recvBuffs;
    std::vector<void *> sendBuffs;

    //read stream remainder tracking
    size_t readHandle;
    size_t readElemsLeft;

    //converter implementations
    double scaleFactor;
    ConvertTypes convertType;
    void convertRecvBuffs(void * const *buffs, const size_t numElems);
    void convertSendBuffs(const void * const *buffs, const size_t numElems);
};
