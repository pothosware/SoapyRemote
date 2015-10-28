// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include "SoapyRemoteConfig.hpp"
#include <cstddef>
#include <string>
#include <vector>

//forward declares
struct sockaddr;

//! A simple storage class for a sockaddr
class SOAPY_REMOTE_API SockAddrData
{
public:
    //! Create an empty socket address
    SockAddrData(void);

    //! Create a socket address from a pointer and length
    SockAddrData(const struct sockaddr *addr, const int addrlen);

    //! Get a pointer to the underlying data
    const struct sockaddr *addr(void) const;

    //! Length of the underlying structure
    size_t addrlen(void) const;

private:
    std::vector<char> _storage;
};

/*!
 * URL parsing, manipulation, lookup.
 */
class SOAPY_REMOTE_API SoapyURL
{
public:
    //! Create empty url object
    SoapyURL(void);

    //! Create URL from components
    SoapyURL(const std::string &scheme, const std::string &node, const std::string &service);

    //! Parse from url markup string
    SoapyURL(const std::string &url);

    //! Create URL from socket address
    SoapyURL(const SockAddrData &addr);

    /*!
     * Convert to socket address + resolve address.
     * Return the error message on failure.
     */
    std::string toSockAddr(SockAddrData &addr) const;

    /*!
     * Convert to URL string markup.
     */
    std::string toString(void) const;

    //! Get the scheme
    std::string getScheme(void) const;

    //! Get the node
    std::string getNode(void) const;

    //! Get the service
    std::string getService(void) const;

    //! Set the scheme
    void setScheme(const std::string &scheme);

    //! Set the node
    void setNode(const std::string &node);

    //! Set the service
    void setService(const std::string &service);

    //! Get the socket type from the scheme
    int getType(void) const;

private:
    std::string _scheme;
    std::string _node;
    std::string _service;
};
