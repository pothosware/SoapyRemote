// Copyright (c) 2015-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <string>

class SoapyRPCSocket;

/*!
 * Create a log acceptor to subscribe to log events from the remote server.
 * The acceptor avoids redundant threads by reference counting subscribers.
 */
class SoapyLogAcceptor
{
public:
    SoapyLogAcceptor(const std::string &url, SoapyRPCSocket &sock, const long timeoutUs = 0);
    ~SoapyLogAcceptor(void);

private:
    std::string _serverId;
};
