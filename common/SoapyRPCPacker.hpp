// Copyright (c) 2015-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include "SoapyRemoteConfig.hpp"
#include <SoapySDR/Types.hpp>
#include <vector>
#include <complex>
#include <string>
#include <stdexcept>

class SoapyRPCSocket;

/*!
 * The packer object accepts primitive Soapy SDR types
 * and encodes them into a portable network RPC format.
 */
class SOAPY_REMOTE_API SoapyRPCPacker
{
public:
    SoapyRPCPacker(SoapyRPCSocket &sock, unsigned int remoteRPCVersion = SoapyRPCVersion);

    ~SoapyRPCPacker(void);

    //! Shortcut operator for send
    void operator()(void)
    {
        this->send();
    }

    //! Send the message when packing is complete
    void send(void);

    //! Pack a binary blob
    void pack(const void *buff, const size_t length);

    //! Pack a single byte
    void pack(const char byte)
    {
        this->ensureSpace(1);
        _message[_size] = byte;
        _size++;
    }

    //! Pack the call
    void operator&(const SoapyRemoteCalls value)
    {
        *this & SOAPY_REMOTE_CALL;
        *this & int(value);
    }

    //! Pack the type
    void operator&(const SoapyRemoteTypes value)
    {
        this->pack(char(value));
    }

    //! Pack a character
    void operator&(const char value);

    //! Pack a boolean
    void operator&(const bool value);

    //! Pack a 32-bit integer
    void operator&(const int value);

    //! Pack a 64-bit integer
    void operator&(const long long value);

    //! Pack a double float
    void operator&(const double value);

    //! Pack a complex double float
    void operator&(const std::complex<double> &value);

    //! Pack a string
    void operator&(const std::string &value);

    //! Pack a range
    void operator&(const SoapySDR::Range &value);

    //! Pack a list of ranges
    void operator&(const SoapySDR::RangeList &value);

    //! Pack a list of strings
    void operator&(const std::vector<std::string> &value);

    //! Pack a list of double floats
    void operator&(const std::vector<double> &value);

    //! Pack a kwargs dictionary
    void operator&(const SoapySDR::Kwargs &value);

    //! Pack a list of kwargs
    void operator&(const SoapySDR::KwargsList &value);

    //! Pack a list of sizes
    void operator&(const std::vector<size_t> &value);

    //! Pack an arg info structure
    void operator&(const SoapySDR::ArgInfo &value);

    //! Pack a list of arg infos
    void operator&(const SoapySDR::ArgInfoList &value);

    //! Pack an exception
    void operator&(const std::exception &value);

private:

    void ensureSpace(const size_t length);

    SoapyRPCSocket &_sock;
    char *_message;
    size_t _size;
    size_t _capacity;
    unsigned int _remoteRPCVersion;
};
