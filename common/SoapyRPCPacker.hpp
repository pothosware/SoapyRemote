// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include "SoapyRemoteCommon.hpp"
#include "SoapyRPCSocket.hpp"
#include <SoapySDR/Types.hpp>
#include <vector>
#include <complex>
#include <string>

/*!
 * The packer object accepts primitive Soapy SDR types
 * and encodes them into a portable network RPC format.
 */
class SoapyRPCPacker
{
public:
    SoapyRPCPacker(SoapyRPCSocket sock);

    ~SoapyRPCPacker(void);

    //! Send the message when packing is complete
    void send(void);

    //! Pack a binary blob
    void pack(const void *buff, const size_t length);

    //! Pack the type
    void operator&(const SoapyRemoteTypes value);

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
    void operator&(const std::vector<SoapySDR::Kwargs> &value);

private:

    void ensureSpace(const size_t length);

    SoapyRPCSocket _sock;
    char *_message;
    size_t _size;
    size_t _capacity;
};
