// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include "SoapyRemoteConfig.hpp"
#include <cstddef>
#include <string>

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

    /*!
     * Make the underlying socket (but does not bind or connect).
     * This function is called automatically by bind and connect,
     * however it can be used to test if a protocol is possible.
     */
    SoapyRPCSocket(const std::string &url);

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

    /*!
     * Get the URL of the local socket.
     * Return an empty string on error.
     */
    std::string getsockname(void);

    /*!
     * Get the URL of the remote socket.
     * Return an empty string on error.
     */
    std::string getpeername(void);

    /*!
     * Set the receive buffer size in bytes.
     * Return the actual size set or negative error code.
     */
    int setRecvBuffSize(const size_t numBytes);

    /*!
     * Set the send buffer size in bytes.
     * Return the actual size set or negative error code.
     */
    int setSendBuffSize(const size_t numBytes);

private:
    int _sock;
};
