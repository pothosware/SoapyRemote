// Copyright (c) 2015-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapySocketDefs.hpp"
#include "SoapyRemoteDefs.hpp"
#include "SoapyRPCSocket.hpp"
#include "SoapyRPCPacker.hpp"
#include <SoapySDR/Version.hpp> //feature defines
#include <cfloat> //DBL_MANT_DIG
#include <cmath> //frexp
#include <cstring> //memcpy
#include <cstdlib> //malloc
#include <algorithm> //min, max
#include <stdexcept>

SoapyRPCPacker::SoapyRPCPacker(SoapyRPCSocket &sock, unsigned int remoteRPCVersion):
    _sock(sock),
    _message(NULL),
    _size(0),
    _capacity(0),
    _remoteRPCVersion(remoteRPCVersion)
{
    //default allocation
    this->ensureSpace(512);

    //allot space for the header (filled in by send)
    SoapyRPCHeader header;
    this->pack(&header, sizeof(header));
}

SoapyRPCPacker::~SoapyRPCPacker(void)
{
    free(_message);
    _message = NULL;
}

void SoapyRPCPacker::send(void)
{
    //load the trailer
    SoapyRPCTrailer trailer;
    trailer.trailerWord = htonl(SoapyRPCTrailerWord);
    this->pack(&trailer, sizeof(trailer));

    //load the header
    SoapyRPCHeader *header = (SoapyRPCHeader *)_message;
    header->headerWord = htonl(SoapyRPCHeaderWord);
    header->version = htonl(SoapyRPCVersion);
    header->length = htonl(_size);

    //send the entire message
    size_t bytesSent = 0;
    while (bytesSent != _size)
    {
        const size_t toSend = std::min<size_t>(SOAPY_REMOTE_SOCKET_BUFFMAX, _size-bytesSent);
        int ret = _sock.send(_message+bytesSent, toSend);
        if (ret < 0)
        {
            throw std::runtime_error("SoapyRPCPacker::send() FAIL: "+std::string(_sock.lastErrorMsg()));
        }
        bytesSent += ret;
    }
}

void SoapyRPCPacker::ensureSpace(const size_t length)
{
    if (_size+length <= _capacity) return;
    const size_t newSize = std::max(_capacity*2, _size+length);
    _message = (char *)realloc(_message, newSize);
}

void SoapyRPCPacker::pack(const void *buff, const size_t length)
{
    this->ensureSpace(length);
    std::memcpy(_message+_size, buff, length);
    _size += length;
}

void SoapyRPCPacker::operator&(const char value)
{
    *this & SOAPY_REMOTE_CHAR;
    this->pack(value);
}

void SoapyRPCPacker::operator&(const bool value)
{
    *this & SOAPY_REMOTE_BOOL;
    char out = value?1:0;
    this->pack(out);
}

void SoapyRPCPacker::operator&(const int value)
{
    *this & SOAPY_REMOTE_INT32;
    int out = htonl(value);
    this->pack(&out, sizeof(out));
}

void SoapyRPCPacker::operator&(const long long value)
{
    *this & SOAPY_REMOTE_INT64;
    long long out = htonll(value);
    this->pack(&out, sizeof(out));
}

void SoapyRPCPacker::operator&(const double value)
{
    *this & SOAPY_REMOTE_FLOAT64;
    int exp = 0;
    const double x = std::frexp(value, &exp);
    const long long man = (long long)std::ldexp(x, DBL_MANT_DIG);
    *this & exp;
    *this & man;
}

void SoapyRPCPacker::operator&(const std::complex<double> &value)
{
    *this & SOAPY_REMOTE_COMPLEX128;
    *this & value.real();
    *this & value.imag();
}

void SoapyRPCPacker::operator&(const std::string &value)
{
    *this & SOAPY_REMOTE_STRING;
    *this & int(value.size());
    this->pack(value.c_str(), value.size());
}

void SoapyRPCPacker::operator&(const SoapySDR::Range &value)
{
    *this & SOAPY_REMOTE_RANGE;
    *this & value.minimum();
    *this & value.maximum();

    //a step size is sent when the remote version matches our current
    if (_remoteRPCVersion >= SoapyRPCVersion)
    {
        #ifdef SOAPY_SDR_API_HAS_RANGE_TYPE_STEP
        *this & value.step();
        #else
        *this & double(0.0);
        #endif
    }
}

void SoapyRPCPacker::operator&(const SoapySDR::RangeList &value)
{
    *this & SOAPY_REMOTE_RANGE_LIST;
    *this & int(value.size());
    for (size_t i = 0; i < value.size(); i++) *this & value[i];
}

void SoapyRPCPacker::operator&(const std::vector<std::string> &value)
{
    *this & SOAPY_REMOTE_STRING_LIST;
    *this & int(value.size());
    for (size_t i = 0; i < value.size(); i++) *this & value[i];
}

void SoapyRPCPacker::operator&(const std::vector<double> &value)
{
    *this & SOAPY_REMOTE_FLOAT64_LIST;
    *this & int(value.size());
    for (size_t i = 0; i < value.size(); i++) *this & value[i];
}

void SoapyRPCPacker::operator&(const SoapySDR::Kwargs &value)
{
    *this & SOAPY_REMOTE_KWARGS;
    *this & int(value.size());
    for (auto it = value.begin(); it != value.end(); ++it)
    {
        *this & it->first;
        *this & it->second;
    }
}

void SoapyRPCPacker::operator&(const SoapySDR::KwargsList &value)
{
    *this & SOAPY_REMOTE_KWARGS_LIST;
    *this & int(value.size());
    for (size_t i = 0; i < value.size(); i++) *this & value[i];
}

void SoapyRPCPacker::operator&(const std::vector<size_t> &value)
{
    *this & SOAPY_REMOTE_SIZE_LIST;
    *this & int(value.size());
    for (size_t i = 0; i < value.size(); i++) *this & int(value[i]);
}

void SoapyRPCPacker::operator&(const SoapySDR::ArgInfo &value)
{
    *this & SOAPY_REMOTE_ARG_INFO;
    *this & value.key;
    *this & value.value;
    *this & value.name;
    *this & value.description;
    *this & value.units;
    *this & int(value.type);
    *this & value.range;
    *this & value.options;
    *this & value.optionNames;
}

void SoapyRPCPacker::operator&(const SoapySDR::ArgInfoList &value)
{
    *this & SOAPY_REMOTE_ARG_INFO_LIST;
    *this & int(value.size());
    for (size_t i = 0; i < value.size(); i++) *this & value[i];
}

void SoapyRPCPacker::operator&(const std::exception &value)
{
    *this & SOAPY_REMOTE_EXCEPTION;
    std::string msg(value.what());
    *this & msg;
}
