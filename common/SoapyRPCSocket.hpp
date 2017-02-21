// Copyright (c) 2015-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include "SoapyRemoteConfig.hpp"
#include <cstddef>
#include <string>

class SockAddrData;

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
     * Connect to client with a timeout is microseconds.
     */
    int connect(const std::string &url, const long timeoutUs);

    //! set or clear non blocking on socket
    int setNonBlocking(const bool nonblock);

    /*!
     * Join a multi-cast group.
     * \param group the url for the multicast group and port number
     * \param loop specify to receive local loopback
     * \param ttl specify time to live for send packets
     * \param iface the IPv6 interface index or 0 for automatic
     */
    int multicastJoin(const std::string &group, const bool loop = true, const int ttl = 1, const int iface = 0);

    /*!
     * Send the buffer and return bytes sent or error.
     */
    int send(const void *buf, size_t len, int flags = 0);

    /*!
     * Receive into buffer and return bytes received or error.
     */
    int recv(void *buf, size_t len, int flags = 0);

    /*!
     * Send to a specific destination.
     */
    int sendto(const void *buf, size_t len, const std::string &url, int flags = 0);

    /*!
     * Receive from an unconnected socket.
     */
    int recvfrom(void *buf, size_t len, std::string &url, int flags = 0);

    /*!
     * Wait for recv to become ready with timeout.
     * Return true for ready, false for timeout.
     */
    bool selectRecv(const long timeoutUs);

    /*!
     * Query the last error message as a string.
     */
    const char *lastErrorMsg(void) const
    {
        return _lastErrorMsg.c_str();
    }

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
     * Set the socket buffer size in bytes.
     * \param isRecv true for RCVBUF, false for SNDBUF
     * \return 0 for success or negative error code.
     */
    int setBuffSize(const bool isRecv, const size_t numBytes);

    /*!
     * Get the socket buffer size in bytes.
     * \param isRecv true for RCVBUF, false for SNDBUF
     * \return the actual size set or negative error code.
     */
    int getBuffSize(const bool isRecv);

private:
    int _sock;
    std::string _lastErrorMsg;

    void reportError(const std::string &what, const std::string &errorMsg);
    void reportError(const std::string &what, const int err);
    void reportError(const std::string &what);
    void setDefaultTcpSockOpts(void);
};
