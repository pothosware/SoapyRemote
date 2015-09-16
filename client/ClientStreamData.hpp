// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include "SoapyRPCSocket.hpp"
#include <vector>
#include <string>

class SoapyRecvEndpoint;
class SoapySendEndpoint;

enum ConvertTypes
{
    CONVERT_MEMCPY,
    CONVERT_CF32_CS16,
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
    SoapyRPCSocket sock;

    //using one of the following endpoints
    SoapyRecvEndpoint *recvEndpoint;
    SoapySendEndpoint *sendEndpoint;

    //buffer pointers to read/write API
    std::vector<const void *> recvBuffs;
    std::vector<void *> sendBuffs;

    //read stream remainder tracking
    size_t readHandle;
    size_t readElemsLeft;

    //converter implementations
    size_t elemSize;
    double scaleFactor;
    ConvertTypes convertType;
    void convertRecvBuffs(void * const *buffs, const size_t numElems);
    void convertSendBuffs(const void * const *buffs, const size_t numElems);
};
