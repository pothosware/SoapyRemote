// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include "SoapyRemoteCommon.hpp"
#include <cstddef>
#include <string>

struct sockaddr;

/*!
 * Utility to parse and lookup a URL string.
 */
bool lookupURL(const std::string &url,
    int &af, int &type, int &prot,
    struct sockaddr &addr, int &addrlen,
    std::string &errorMsg);

/*!
 * Create one instance of the session per process to use sockets.
 */
class SOAPY_REMOTE_API SoapySocketSession
{
public:
    SoapySocketSession(void);
    ~SoapySocketSession(void);
};

/*!
 * A simple socket wrapper with a TCP-like socket API.
 * The implementation may be swapped out in the future.
 */
class SOAPY_REMOTE_API SoapyRPCSocket
{
public:
    SoapyRPCSocket(void);

    ~SoapyRPCSocket(void);

    /*!
     * Is the socket null?
     * The default constructor makes a null socket.
     * The socket is non null after bind or connect,
     * and after accept returns a successful socket.
     */
    bool null(void);

    /*!
     * Explicit close the socket, also done by destructor.
     */
    int close(void);

    /*!
     * Server bind.
     * URL examples:
     * 0.0.0.0:1234
     * [::]:1234
     */
    int bind(const std::string &url);

    /*!
     * Server listen.
     */
    int listen(int backlog);

    /*!
     * Server accept connection.
     * Socket will be null on failure.
     * Caller owns the client socket.
     */
    SoapyRPCSocket *accept(void);

    /*!
     * Client connect.
     * URL examples:
     * 10.10.1.123:1234
     * [2001:db8:0:1]:1234
     * hostname:1234
     */
    int connect(const std::string &url);

    /*!
     * Send the buffer and return bytes sent or error.
     */
    int send(const void *buf, size_t len, int flags = 0);

    /*!
     * Receive into buffer and return bytes received or error.
     */
    int recv(void *buf, size_t len, int flags = 0);

    /*!
     * Wait for recv to become ready with timeout.
     * Return true for ready, false for timeout.
     */
    bool selectRecv(const long timeoutUs);

    /*!
     * Query the last error message as a string.
     */
    const char *lastErrorMsg(void);

private:
    int _sock;
};
