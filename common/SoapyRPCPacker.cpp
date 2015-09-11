// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyNetworkDefs.hpp"
#include "SoapyRPCPacker.hpp"
#include <cfloat> //DBL_MANT_DIG
#include <cmath> //frexp
#include <cstring> //memcpy
#include <cstdlib> //malloc
#include <algorithm> //max
#include <udt.h> //htonl

SoapyRPCPacker::SoapyRPCPacker(SoapyRPCSocket sock):
    _sock(sock),
    _message(NULL),
    _size(0),
    _capacity(0)
{
    return;
}

SoapyRPCPacker::~SoapyRPCPacker(void)
{
    free(_message);
    _message = NULL;
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

void SoapyRPCPacker::operator&(const SoapyRemoteTypes value)
{
    this->ensureSpace(1);
    _message[_size] = char(value);
    _size++;
}

void SoapyRPCPacker::operator&(const char value)
{
    *this & SOAPY_REMOTE_CHAR;
    this->pack(&value, sizeof(value));
}

void SoapyRPCPacker::operator&(const bool value)
{
    *this & SOAPY_REMOTE_BOOL;
    char out = value?1:0;
    this->pack(&out, sizeof(out));
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
    const long long scale = ((long long)(1)) << DBL_MANT_DIG;
    const long long man = (long long)(scale*x);
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
    this->pack(value.c_str(), value.size());
}

void SoapyRPCPacker::operator&(const SoapySDR::Range &value)
{
    *this & SOAPY_REMOTE_RANGE;
    *this & value.minimum();
    *this & value.maximum();
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
    for (SoapySDR::Kwargs::const_iterator it = value.begin(); it != value.end(); ++it)
    {
        *this & it->first;
        *this & it->second;
    }
}

void SoapyRPCPacker::operator&(const std::vector<SoapySDR::Kwargs> &value)
{
    *this & SOAPY_REMOTE_KWARGS_LIST;
    *this & int(value.size());
    for (size_t i = 0; i < value.size(); i++) *this & value[i];
}
