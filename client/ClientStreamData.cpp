// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "ClientStreamData.hpp"
#include "SoapyStreamEndpoint.hpp"
#include <cstring> //memcpy
#include <cassert>
#include <cstdint>

ClientStreamData::ClientStreamData(void):
    streamId(-1),
    endpoint(nullptr),
    readHandle(0),
    readElemsLeft(0),
    scaleFactor(0.0),
    convertType(CONVERT_MEMCPY)
{
    return;
}

void ClientStreamData::convertRecvBuffs(void * const *buffs, const size_t numElems)
{
    assert(endpoint != nullptr);
    assert(endpoint->getElemSize() != 0);
    assert(endpoint->getNumChans() != 0);
    assert(not recvBuffs.empty());

    switch (convertType)
    {
    ///////////////////////////
    case CONVERT_MEMCPY:
    ///////////////////////////
    {
        size_t elemSize = endpoint->getElemSize();
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
        const float scale = float(1.0/scaleFactor);
        for (size_t i = 0; i < recvBuffs.size(); i++)
        {
            for (size_t j = 0; j < numElems*2; j++)
            {
                auto in = (short *)recvBuffs[i];
                auto out = (float *)buffs[i];
                for (size_t j = 0; j < numElems*2; j++)
                {
                    out[j] = float(in[j])*scale;
                }
            }
        }
    }

    ///////////////////////////
    case CONVERT_CF32_CS8:
    ///////////////////////////
    {
        const float scale = float(1.0/scaleFactor);
        for (size_t i = 0; i < recvBuffs.size(); i++)
        {
            for (size_t j = 0; j < numElems*2; j++)
            {
                auto in = (int8_t *)recvBuffs[i];
                auto out = (float *)buffs[i];
                for (size_t j = 0; j < numElems*2; j++)
                {
                    out[j] = float(in[j])*scale;
                }
            }
        }
    }
    break;
    }
}

void ClientStreamData::convertSendBuffs(const void * const *buffs, const size_t numElems)
{
    assert(endpoint != nullptr);
    assert(endpoint->getElemSize() != 0);
    assert(endpoint->getNumChans() != 0);
    assert(not sendBuffs.empty());

    switch (convertType)
    {
    ///////////////////////////
    case CONVERT_MEMCPY:
    ///////////////////////////
    {
        size_t elemSize = endpoint->getElemSize();
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
        float scale = float(scaleFactor);
        for (size_t i = 0; i < sendBuffs.size(); i++)
        {
            auto in = (float *)buffs[i];
            auto out = (short *)sendBuffs[i];
            for (size_t j = 0; j < numElems*2; j++)
            {
                out[j] = short(in[j]*scale);
            }
        }
    }

    ///////////////////////////
    case CONVERT_CF32_CS8:
    ///////////////////////////
    {
        float scale = float(scaleFactor);
        for (size_t i = 0; i < sendBuffs.size(); i++)
        {
            auto in = (float *)buffs[i];
            auto out = (int8_t *)sendBuffs[i];
            for (size_t j = 0; j < numElems*2; j++)
            {
                out[j] = int8_t(in[j]*scale);
            }
        }
    }
    break;
    }
}
