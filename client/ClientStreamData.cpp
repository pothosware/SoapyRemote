// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "ClientStreamData.hpp"
#include <cstring> //memcpy

ClientStreamData::ClientStreamData(void):
    streamId(-1),
    recvEndpoint(nullptr),
    sendEndpoint(nullptr),
    readHandle(0),
    readElemsLeft(0),
    elemSize(0),
    scaleFactor(0.0),
    convertType(CONVERT_MEMCPY)
{
    return;
}

void ClientStreamData::convertRecvBuffs(void * const *buffs, const size_t numElems)
{
    switch (convertType)
    {
    ///////////////////////////
    case CONVERT_MEMCPY:
    ///////////////////////////
    {
        for (size_t i = 0; i < recvBuffs.size(); i++)
        {
            std::memcpy(buffs[i], recvBuffs[i], numElems*elemSize);
        }
    }
    break;

    ///////////////////////////
    case CONVERT_CF32_CS16:
    ///////////////////////////
    {
        const float scale = float(scaleFactor);
        for (size_t i = 0; i < recvBuffs.size(); i++)
        {
            for (size_t j = 0; j < numElems*2; j++)
            {
                auto in = (short *)recvBuffs[i];
                auto out = (float *)buffs[i];
                for (size_t j = 0; j < numElems*2; j++)
                {
                    out[j] = float(in[i])*scale;
                }
            }
        }
    }
    break;
    }
}

void ClientStreamData::convertSendBuffs(const void * const *buffs, const size_t numElems)
{
    switch (convertType)
    {
    ///////////////////////////
    case CONVERT_MEMCPY:
    ///////////////////////////
    {
        for (size_t i = 0; i < sendBuffs.size(); i++)
        {
            std::memcpy(sendBuffs[i], buffs[i], numElems*elemSize);
        }
    }
    break;

    ///////////////////////////
    case CONVERT_CF32_CS16:
    ///////////////////////////
    {
        float scale = float(1.0/scaleFactor);
        for (size_t i = 0; i < sendBuffs.size(); i++)
        {
            auto in = (float *)buffs[i];
            auto out = (short *)sendBuffs[i];
            for (size_t j = 0; j < numElems*2; j++)
            {
                out[j] = short(in[i]*scale);
            }
        }
    }
    break;
    }
}
