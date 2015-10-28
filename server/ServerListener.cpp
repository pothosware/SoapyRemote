// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyServer.hpp"
#include "SoapyRemoteDefs.hpp"
#include "ClientHandler.hpp"
#include "SoapyRPCSocket.hpp"
#include <thread>
#include <iostream>

/***********************************************************************
 * Server thread implementation
 **********************************************************************/
SoapyServerThreadData::SoapyServerThreadData(void):
    done(false),
    thread(nullptr),
    client(nullptr)
{
    return;
}

SoapyServerThreadData::~SoapyServerThreadData(void)
{
    done = true;
    if (thread != nullptr)
    {
        thread->join();
    }
    delete thread;
    if (client != nullptr)
    {
        std::cout << "SoapyServerListener::close()" << std::endl;
    }
    delete client;
}

void SoapyServerThreadData::handlerLoop(void)
{
    SoapyClientHandler handler(*client, uuid);

    try
    {
        while (handler.handleOnce())
        {
            if (done) break;
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << "SoapyServerListener::handlerLoop() FAIL: " << ex.what() << std::endl;
    }

    done = true;
}

/***********************************************************************
 * Socket listener constructor
 **********************************************************************/
SoapyServerListener::SoapyServerListener(SoapyRPCSocket &sock, const std::string &uuid):
    _sock(sock),
    _uuid(uuid),
    _handlerId(0)
{
    return;
}

SoapyServerListener::~SoapyServerListener(void)
{
    auto it = _handlers.begin();
    while (it != _handlers.end())
    {
        _handlers.erase(it++);
    }
}

/***********************************************************************
 * Client socket acceptor
 **********************************************************************/
void SoapyServerListener::handleOnce(void)
{
    //cleanup completed threads
    auto it = _handlers.begin();
    while (it != _handlers.end())
    {
        auto &data = it->second;
        if (not data.done) ++it;
        else _handlers.erase(it++);
    }

    //wait with timeout for the server socket to become ready to accept
    if (not _sock.selectRecv(SOAPY_REMOTE_SOCKET_TIMEOUT_US)) return;

    SoapyRPCSocket *client = _sock.accept();
    if (client == NULL)
    {
        std::cerr << "SoapyServerListener::accept() FAIL:" << _sock.lastErrorMsg() << std::endl;
        return;
    }
    std::cout << "SoapyServerListener::accept(" << client->getpeername() << ")" << std::endl;

    //setup the thread data
    auto &data = _handlers[_handlerId++];
    data.client = client;
    data.uuid = _uuid;

    //spawn a new thread
    data.thread = new std::thread(&SoapyServerThreadData::handlerLoop, &data);
}
