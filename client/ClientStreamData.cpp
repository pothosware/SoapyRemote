// Copyright (c) 2015-2017 Josh Blum
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
            auto in = (short *)recvBuffs[i];
            auto out = (float *)buffs[i];
            for (size_t j = 0; j < numElems*2; j++)
            {
                out[j] = float(in[j])*scale;
            }
        }
    }
    break;

    ///////////////////////////
    case CONVERT_CF32_CS12:
    ///////////////////////////
    {
        // note that we correct the scale for the CS16 intermediate step
        const float scale = float(1.0/16.0/scaleFactor);
        for (size_t i = 0; i < recvBuffs.size(); i++)
        {
            auto in = (uint8_t *)recvBuffs[i];
            auto out = (float *)buffs[i];
            for (size_t j = 0; j < numElems; j++)
            {
                uint16_t part0 = uint16_t(*(in++));
                uint16_t part1 = uint16_t(*(in++));
                uint16_t part2 = uint16_t(*(in++));
                int16_t i = int16_t((part1 << 12) | (part0 << 4));
                int16_t q = int16_t((part2 << 8) | (part1 & 0xf0));
                *(out++) = float(i)*scale;
                *(out++) = float(q)*scale;
            }
        }
    }
    break;

    ///////////////////////////
    case CONVERT_CS16_CS12:
    ///////////////////////////
    {
        for (size_t i = 0; i < recvBuffs.size(); i++)
        {
            auto in = (uint8_t *)recvBuffs[i];
            auto out = (int16_t *)buffs[i];
            for (size_t j = 0; j < numElems; j++)
            {
                uint16_t part0 = uint16_t(*(in++));
                uint16_t part1 = uint16_t(*(in++));
                uint16_t part2 = uint16_t(*(in++));
                *(out++) = int16_t((part1 << 12) | (part0 << 4));
                *(out++) = int16_t((part2 << 8) | (part1 & 0xf0));
            }
        }
    }
    break;

    ///////////////////////////
    case CONVERT_CS16_CS8:
    ///////////////////////////
    {
        for (size_t i = 0; i < recvBuffs.size(); i++)
        {
            auto in = (int8_t *)recvBuffs[i];
            auto out = (int16_t *)buffs[i];
            for (size_t j = 0; j < numElems*2; j++)
            {
                out[j] = int16_t(in[j]);
            }
        }
    }
    break;

    ///////////////////////////
    case CONVERT_CF32_CS8:
    ///////////////////////////
    {
        const float scale = float(1.0/scaleFactor);
        for (size_t i = 0; i < recvBuffs.size(); i++)
        {
            auto in = (int8_t *)recvBuffs[i];
            auto out = (float *)buffs[i];
            for (size_t j = 0; j < numElems*2; j++)
            {
                out[j] = float(in[j])*scale;
            }
        }
    }
    break;

    ///////////////////////////
    case CONVERT_CF32_CU8:
    ///////////////////////////
    {
        const float scale = float(1.0/scaleFactor);
        for (size_t i = 0; i < recvBuffs.size(); i++)
        {
            auto in = (int8_t *)recvBuffs[i];
            auto out = (float *)buffs[i];
            for (size_t j = 0; j < numElems*2; j++)
            {
                out[j] = float(in[j]-127)*scale;
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
    break;

    ///////////////////////////
    case CONVERT_CF32_CS12:
    ///////////////////////////
    {
        // note that we correct the scale for the CS16 intermediate step
        const float scale = float(16.0*scaleFactor);
        for (size_t i = 0; i < sendBuffs.size(); i++)
        {
            auto in = (float *)buffs[i];
            auto out = (uint8_t *)sendBuffs[i];
            for (size_t j = 0; j < numElems; j++)
            {
                uint16_t i = uint16_t(*(in++)*scale);
                uint16_t q = uint16_t(*(in++)*scale);
                *(out++) = uint8_t(i >> 4);
                *(out++) = uint8_t((q & 0xf0)|(i >> 12));
                *(out++) = uint8_t(q >> 8);
            }
        }
    }
    break;

    ///////////////////////////
    case CONVERT_CS16_CS12:
    ///////////////////////////
    {
        for (size_t i = 0; i < sendBuffs.size(); i++)
        {
            auto in = (int16_t *)buffs[i];
            auto out = (uint8_t *)sendBuffs[i];
            for (size_t j = 0; j < numElems; j++)
            {
                uint16_t i = uint16_t(*(in++));
                uint16_t q = uint16_t(*(in++));
                *(out++) = uint8_t(i >> 4);
                *(out++) = uint8_t((q & 0xf0)|(i >> 12));
                *(out++) = uint8_t(q >> 8);
            }
        }
    }
    break;

    ///////////////////////////
    case CONVERT_CS16_CS8:
    ///////////////////////////
    {
        for (size_t i = 0; i < sendBuffs.size(); i++)
        {
            auto in = (int16_t *)buffs[i];
            auto out = (int8_t *)sendBuffs[i];
            for (size_t j = 0; j < numElems*2; j++)
            {
                out[j] = int8_t(in[j]);
            }
        }
    }
    break;

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

    ///////////////////////////
    case CONVERT_CF32_CU8:
    ///////////////////////////
    {
        float scale = float(scaleFactor);
        for (size_t i = 0; i < sendBuffs.size(); i++)
        {
            auto in = (float *)buffs[i];
            auto out = (int8_t *)sendBuffs[i];
            for (size_t j = 0; j < numElems*2; j++)
            {
                out[j] = int8_t(in[j]*scale) + 127;
            }
        }
    }
    break;
    }
}
