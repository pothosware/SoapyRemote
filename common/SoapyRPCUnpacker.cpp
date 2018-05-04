// Copyright (c) 2015-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapySocketDefs.hpp"
#include "SoapyRemoteDefs.hpp"
#include "SoapyRPCSocket.hpp"
#include "SoapyRPCUnpacker.hpp"
#include "SoapyRPCPacker.hpp"
#include <SoapySDR/Logger.hpp>
#include <SoapySDR/Version.hpp> //feature defines
#include <cfloat> //DBL_MANT_DIG
#include <cmath> //ldexp
#include <cstring> //memcpy
#include <cstdlib> //malloc
#include <algorithm> //min, max
#include <stdexcept>
#include <chrono>

//! How long to wait for the server presence checks
static const long SERVER_CHECK_TIMEOUT_US = 3000000; //3 seconds

static void testServerConnection(const std::string &url)
{
    SoapyRPCSocket s;
    int ret = s.connect(url, SERVER_CHECK_TIMEOUT_US);
    if (ret != 0) throw std::runtime_error("SoapyRPCUnpacker::recv() FAIL test server connection: "+std::string(s.lastErrorMsg()));
    SoapyRPCPacker packerHangup(s);
    packerHangup & SOAPY_REMOTE_HANGUP;
    packerHangup();
    s.selectRecv(SERVER_CHECK_TIMEOUT_US);
}

SoapyRPCUnpacker::SoapyRPCUnpacker(SoapyRPCSocket &sock, const bool autoRecv, const long timeoutUs):
    _sock(sock),
    _message(NULL),
    _offset(0),
    _capacity(0),
    _remoteRPCVersion(SoapyRPCVersion)
{
    //auto recv expects a reply packet within a reasonable time window
    //or else the link might be down, in which case we throw an error.
    //Calls are allowed to take a long time (up to 31 seconds).
    //However, we continually check that the server is active
    //so that we can tear down immediately if the server goes away.
    if (timeoutUs >= SERVER_CHECK_TIMEOUT_US)
    {
        const auto exitTime = std::chrono::high_resolution_clock::now() + std::chrono::microseconds(timeoutUs);
        while (not _sock.selectRecv(SERVER_CHECK_TIMEOUT_US))
        {
            testServerConnection(_sock.getpeername());
            if (std::chrono::high_resolution_clock::now() > exitTime)
                throw std::runtime_error("SoapyRPCUnpacker::recv() TIMEOUT");
        }
    }
    //small timeout but not -1 for infinite timeout
    else if (timeoutUs >= 0 and not _sock.selectRecv(timeoutUs))
    {
        throw std::runtime_error("SoapyRPCUnpacker::recv() TIMEOUT");
    }

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
    int ret = _sock.recv(&header, sizeof(header), MSG_WAITALL);
    if (ret != sizeof(header))
    {
        throw std::runtime_error("SoapyRPCUnpacker::recv(header) FAIL: "+std::string(_sock.lastErrorMsg()));
    }

    //inspect and parse the header
    if (ntohl(header.headerWord) != SoapyRPCHeaderWord)
    {
        throw std::runtime_error("SoapyRPCUnpacker::recv() FAIL: header word");
    }
    _remoteRPCVersion = ntohl(header.version);
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
    size_t bytesReceived = 0;
    while (bytesReceived != _capacity)
    {
        const size_t toRecv = std::min<size_t>(SOAPY_REMOTE_SOCKET_BUFFMAX, _capacity-bytesReceived);
        ret = _sock.recv(_message+bytesReceived, toRecv);
        if (ret < 0)
        {
            throw std::runtime_error("SoapyRPCUnpacker::recv(payload) FAIL: "+std::string(_sock.lastErrorMsg()));
        }
        bytesReceived += ret;
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
        throw std::runtime_error("RemoteError: "+errorMsg);
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
    double minimum = 0.0, maximum = 0.0, step = 0.0;
    *this & minimum;
    *this & maximum;

    //a step size is sent when the remote version matches our current
    if (_remoteRPCVersion >= SoapyRPCVersion)
    {
        *this & step;
    }

    #ifdef SOAPY_SDR_API_HAS_RANGE_TYPE_STEP
    value = SoapySDR::Range(minimum, maximum, step);
    #else
    value = SoapySDR::Range(minimum, maximum);
    #endif
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

void SoapyRPCUnpacker::operator&(SoapySDR::KwargsList &value)
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
    for (size_t i = 0; i < value.size(); i++)
    {
        *this & size;
        value[i] = size;
    }
}

void SoapyRPCUnpacker::operator&(SoapySDR::ArgInfo &value)
{
    UNPACK_TYPE_HELPER(SOAPY_REMOTE_ARG_INFO);
    *this & value.key;
    *this & value.value;
    *this & value.name;
    *this & value.description;
    *this & value.units;
    int intType = 0; *this & intType;
    value.type = SoapySDR::ArgInfo::Type(intType);
    *this & value.range;
    *this & value.options;
    *this & value.optionNames;
}

void SoapyRPCUnpacker::operator&(SoapySDR::ArgInfoList &value)
{
    UNPACK_TYPE_HELPER(SOAPY_REMOTE_ARG_INFO_LIST);
    int size = 0;
    *this & size;
    value.resize(size);
    for (size_t i = 0; i < size_t(size); i++) *this & value[i];
}
