// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyServer.hpp"
#include "SoapyRPCPacker.hpp"
#include "SoapyRPCUnpacker.hpp"
#include <iostream>
#include <pthread.h>

/***********************************************************************
 * The device factor make and unmake requires a process-wide mutex
 **********************************************************************/
static pthread_mutex_t factoryMutex = PTHREAD_MUTEX_INITIALIZER;

struct FactoryLock
{
    FactoryLock(void)
    {
        pthread_mutex_lock(&factoryMutex);
    }
    ~FactoryLock(void)
    {
        pthread_mutex_unlock(&factoryMutex);
    }
};

/***********************************************************************
 * Handler dispatcher implementation
 **********************************************************************/
SoapyClientHandler::SoapyClientHandler(SoapyRPCSocket &sock):
    _sock(sock),
    _dev(NULL)
{
    return;
}

SoapyClientHandler::~SoapyClientHandler(void)
{
    if (_dev != NULL)
    {
        FactoryLock lock;
        SoapySDR::Device::unmake(_dev);
        _dev = NULL;
    }
}

bool SoapyClientHandler::handleOnce(void)
{
    //receive the client's request
    SoapyRPCUnpacker unpacker(_sock);
    SoapyRPCPacker packer(_sock);

    //handle the client's request
    bool again = true;
    try
    {
        again = this->handleOnce(unpacker, packer);
    }
    catch (const std::exception &ex)
    {
        packer & ex;
    }

    //send the result back
    packer();

    return again;
}

bool SoapyClientHandler::handleOnce(SoapyRPCUnpacker &unpacker, SoapyRPCPacker &packer)
{
    SoapyRemoteCalls call;
    unpacker & call;

    switch (call)
    {

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_FIND:
    ////////////////////////////////////////////////////////////////////
    {
        SoapySDR::Kwargs args;
        unpacker & args;

        //stop infinite loops with special keyword
        args[SOAPY_REMOTE_KWARG_STOP] = "";

        //rewrite driver from remoteDriver
        args.erase("driver");
        if (args.count("remoteDriver") != 0)
        {
            args["driver"] = args["remoteDriver"];
            args.erase("remoteDriver");
        }

        std::vector<SoapySDR::Kwargs> result = SoapySDR::Device::enumerate(args);
        packer & result;

        //one-time use socket
        return false;
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_MAKE:
    ////////////////////////////////////////////////////////////////////
    {
        SoapySDR::Kwargs args;
        unpacker & args;

        //rewrite driver from remoteDriver
        args.erase("driver");
        if (args.count("remoteDriver") != 0)
        {
            args["driver"] = args["remoteDriver"];
            args.erase("remoteDriver");
        }

        FactoryLock lock;
        _dev = SoapySDR::Device::make(args);
        packer & SOAPY_REMOTE_VOID;
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_UNMAKE:
    ////////////////////////////////////////////////////////////////////
    {
        FactoryLock lock;
        if (_dev != NULL) SoapySDR::Device::unmake(_dev);
        _dev = NULL;
        packer & SOAPY_REMOTE_VOID;

        //client is done with the socket
        return false;
    } break;

    default: break; //TODO unknown...
    }

    return true;
}
