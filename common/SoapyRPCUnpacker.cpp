// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapySocketDefs.hpp"
#include "SoapyRemoteDefs.hpp"
#include "SoapyRPCSocket.hpp"
#include "SoapyRPCUnpacker.hpp"
#include <SoapySDR/Logger.hpp>
#include <cfloat> //DBL_MANT_DIG
#include <cmath> //ldexp
#include <cstring> //memcpy
#include <cstdlib> //malloc
#include <algorithm> //max
#include <stdexcept>

SoapyRPCUnpacker::SoapyRPCUnpacker(SoapyRPCSocket &sock, const bool autoRecv):
    _sock(sock),
    _message(NULL),
    _offset(0),
    _capacity(0)
{
    if (autoRecv) this->recv();
}

SoapyRPCUnpacker::~SoapyRPCUnpacker(void)
{
    free(_message);
    _message = NULL;
    _offset += sizeof(SoapyRPCTrailer); //consume trailer
    if (_offset != _capacity)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "~SoapyRPCUnpacker: Unconsumed payload bytes %d", int(_capacity-_offset));
    }
}

void SoapyRPCUnpacker::recv(void)
{
    //receive the header
    SoapyRPCHeader header;
    int ret = _sock.recv(&header, sizeof(header));
    if (ret != sizeof(header))
    {
        throw std::runtime_error("SoapyRPCUnpacker::recv(header) FAIL: "+std::string(_sock.lastErrorMsg()));
    }

    //inspect and parse the header
    if (ntohl(header.headerWord) != SoapyRPCHeaderWord)
    {
        throw std::runtime_error("SoapyRPCUnpacker::recv() FAIL: header word");
    }
    //TODO ignoring the version for now
    //the check may need to be delicate with the version major, minor vs patch number
    const size_t length = ntohl(header.length);
    if (length <= sizeof(SoapyRPCHeader) + sizeof(SoapyRPCTrailer))
    {
        throw std::runtime_error("SoapyRPCUnpacker::recv() FAIL: header length");
    }

    //receive the remaining payload
    _capacity = length - sizeof(SoapyRPCHeader);
    _message = (char *)malloc(_capacity);
    ret = _sock.recv(_message, _capacity);
    if (ret != int(_capacity))
    {
        throw std::runtime_error("SoapyRPCUnpacker::recv(payload) FAIL: "+std::string(_sock.lastErrorMsg()));
    }

    //check the trailer
    SoapyRPCTrailer trailer;
    std::memcpy(&trailer, _message + _capacity - sizeof(SoapyRPCTrailer), sizeof(trailer));
    if (ntohl(trailer.trailerWord) != SoapyRPCTrailerWord)
    {
        throw std::runtime_error("SoapyRPCUnpacker::recv() FAIL: trailer word");
    }

    //auto-consume void
    if (this->peekType() == SOAPY_REMOTE_VOID)
    {
        SoapyRemoteTypes type;
        *this & type;
    }

    //check for exceptions
    else if (this->peekType() == SOAPY_REMOTE_EXCEPTION)
    {
        SoapyRemoteTypes type;
        std::string errorMsg;
        *this & type;
        *this & errorMsg;
        throw std::runtime_error(errorMsg);
    }
}

void SoapyRPCUnpacker::unpack(void *buff, const size_t length)
{
    std::memcpy(buff, this->unpack(length), length);
}

void *SoapyRPCUnpacker::unpack(const size_t length)
{
    if (_offset + length > _capacity - sizeof(SoapyRPCTrailer))
    {
        throw std::runtime_error("SoapyRPCUnpacker::unpack() OVER-CONSUME");
    }
    void *buff = _message+_offset;
    _offset += length;
    return buff;
}

bool SoapyRPCUnpacker::done(void) const
{
    return (_offset + sizeof(SoapyRPCTrailer)) == _capacity;
}

#define UNPACK_TYPE_HELPER(expected) \
    SoapyRemoteTypes type; *this & type; \
    if (type != expected) {throw std::runtime_error("SoapyRPCUnpacker type check FAIL:" #expected);} else {}

void SoapyRPCUnpacker::operator&(SoapyRemoteCalls &value)
{
    UNPACK_TYPE_HELPER(SOAPY_REMOTE_CALL);
    int call = 0;
    *this & call;
    value = SoapyRemoteCalls(call);
}

void SoapyRPCUnpacker::operator&(char &value)
{
    UNPACK_TYPE_HELPER(SOAPY_REMOTE_CHAR);
    value = this->unpack();
}

void SoapyRPCUnpacker::operator&(bool &value)
{
    UNPACK_TYPE_HELPER(SOAPY_REMOTE_BOOL);
    char in = this->unpack();
    value = (in == 0)?false:true;
}

void SoapyRPCUnpacker::operator&(int &value)
{
    UNPACK_TYPE_HELPER(SOAPY_REMOTE_INT32);
    this->unpack(&value, sizeof(value));
    value = ntohl(value);
}

void SoapyRPCUnpacker::operator&(long long &value)
{
    UNPACK_TYPE_HELPER(SOAPY_REMOTE_INT64);
    this->unpack(&value, sizeof(value));
    value = ntohll(value);
}

void SoapyRPCUnpacker::operator&(double &value)
{
    UNPACK_TYPE_HELPER(SOAPY_REMOTE_FLOAT64);
    int exp = 0;
    long long man = 0;
    *this & exp;
    *this & man;
    value = std::ldexp(double(man), exp-DBL_MANT_DIG);
}

void SoapyRPCUnpacker::operator&(std::complex<double> &value)
{
    UNPACK_TYPE_HELPER(SOAPY_REMOTE_COMPLEX128);
    double r = 0.0, i = 0.0;
    *this & r;
    *this & i;
    value = std::complex<double>(r, i);
}

void SoapyRPCUnpacker::operator&(std::string &value)
{
    UNPACK_TYPE_HELPER(SOAPY_REMOTE_STRING);
    int size = 0;
    *this & size;
    value = std::string((const char *)this->unpack(size), size);
}

void SoapyRPCUnpacker::operator&(SoapySDR::Range &value)
{
    UNPACK_TYPE_HELPER(SOAPY_REMOTE_RANGE);
    double minimum = 0.0, maximum = 0.0;
    *this & minimum;
    *this & maximum;
    value = SoapySDR::Range(minimum, maximum);
}

void SoapyRPCUnpacker::operator&(SoapySDR::RangeList &value)
{
    UNPACK_TYPE_HELPER(SOAPY_REMOTE_RANGE_LIST);
    int size = 0;
    *this & size;
    value.resize(size);
    for (size_t i = 0; i < size_t(size); i++) *this & value[i];
}

void SoapyRPCUnpacker::operator&(std::vector<std::string> &value)
{
    UNPACK_TYPE_HELPER(SOAPY_REMOTE_STRING_LIST);
    int size = 0;
    *this & size;
    value.resize(size);
    for (size_t i = 0; i < size_t(size); i++) *this & value[i];
}

void SoapyRPCUnpacker::operator&(std::vector<double> &value)
{
    UNPACK_TYPE_HELPER(SOAPY_REMOTE_FLOAT64_LIST);
    int size = 0;
    *this & size;
    value.resize(size);
    for (size_t i = 0; i < size_t(size); i++) *this & value[i];
}

void SoapyRPCUnpacker::operator&(SoapySDR::Kwargs &value)
{
    UNPACK_TYPE_HELPER(SOAPY_REMOTE_KWARGS);
    int size = 0;
    *this & size;
    value.clear();
    for (size_t i = 0; i < size_t(size); i++)
    {
        std::string key, val;
        *this & key;
        *this & val;
        value[key] = val;
    }
}

void SoapyRPCUnpacker::operator&(std::vector<SoapySDR::Kwargs> &value)
{
    UNPACK_TYPE_HELPER(SOAPY_REMOTE_KWARGS_LIST);
    int size = 0;
    *this & size;
    value.resize(size);
    for (size_t i = 0; i < size_t(size); i++) *this & value[i];
}

void SoapyRPCUnpacker::operator&(std::vector<size_t> &value)
{
    UNPACK_TYPE_HELPER(SOAPY_REMOTE_SIZE_LIST);
    int size = 0;
    *this & size;
    value.resize(size);
    for (size_t i = 0; i < size_t(size); i++)
    {
        *this & size;
        value[i] = size;
    }
}
