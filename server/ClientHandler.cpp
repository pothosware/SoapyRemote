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
    SoapyRPCUnpacker unpack(_sock);
    SoapyRemoteCalls call; unpack & call;

    switch (call)
    {

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_FIND:
    ////////////////////////////////////////////////////////////////////
    {
        SoapySDR::Kwargs args;
        unpack & args;

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
        SoapyRPCPacker packer(_sock);
        packer & result;
        packer();

        return false; //find is a one-use socket
    }

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_MAKE:
    ////////////////////////////////////////////////////////////////////
    {
        SoapySDR::Kwargs args;
        unpack & args;
        {
            FactoryLock lock;
            //TODO pass exception
            _dev = SoapySDR::Device::make(args);
        }

        SoapyRPCPacker packer(_sock);
        packer();
    }

    default: break; //TODO unknown...
    }
    return true;
}
