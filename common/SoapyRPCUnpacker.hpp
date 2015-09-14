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
 * The unpacker object receives a complete RPC message,
 * and unpacks the message into primitive Soapy SDR types.
 */
class SOAPY_REMOTE_API SoapyRPCUnpacker
{
public:
    SoapyRPCUnpacker(SoapyRPCSocket &sock, const bool autoRecv = true);

    ~SoapyRPCUnpacker(void);

    //! Receive a complete RPC message
    void recv(void);

    //! Unpack a binary blob of known size
    void unpack(void *buff, const size_t length);

    //! Copy-less version of unpack
    void *unpack(const size_t length);

    //! Unpack a single byte
    char unpack(void)
    {
        char byte = _message[_offset];
        _offset++;
        return byte;
    }

    //! Done when no data is left to unpack
    bool done(void) const;

    //! View the next type without consuming
    SoapyRemoteTypes peekType(void) const
    {
        return SoapyRemoteTypes(_message[_offset]);
    }

    //! Unpack the call
    void operator&(SoapyRemoteCalls &value);

    //! Unpack the type
    void operator&(SoapyRemoteTypes &value)
    {
        value = SoapyRemoteTypes(this->unpack());
    }

    //! Unpack a character
    void operator&(char &value);

    //! Unpack a boolean
    void operator&(bool &value);

    //! Unpack a 32-bit integer
    void operator&(int &value);

    //! Unpack a 64-bit integer
    void operator&(long long &value);

    //! Unpack a double float
    void operator&(double &value);

    //! Unpack a complex double float
    void operator&(std::complex<double> &value);

    //! Unpack a string
    void operator&(std::string &value);

    //! Unpack a range
    void operator&(SoapySDR::Range &value);

    //! Unpack a list of ranges
    void operator&(SoapySDR::RangeList &value);

    //! Unpack a list of strings
    void operator&(std::vector<std::string> &value);

    //! Unpack a list of double floats
    void operator&(std::vector<double> &value);

    //! Unpack a kwargs dictionary
    void operator&(SoapySDR::Kwargs &value);

    //! Unpack a list of kwargs
    void operator&(std::vector<SoapySDR::Kwargs> &value);

private:

    void ensureSpace(const size_t length);

    SoapyRPCSocket &_sock;
    char *_message;
    size_t _offset;
    size_t _capacity;
};
