// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyServer.hpp"
#include "SoapyRPCPacker.hpp"
#include "SoapyRPCUnpacker.hpp"
#include <iostream>
#include <mutex>

//! The device factory make and unmake requires a process-wide mutex
static std::mutex factoryMutex;

/***********************************************************************
 * Client handler constructor
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
        std::lock_guard<std::mutex> lock(factoryMutex);
        SoapySDR::Device::unmake(_dev);
        _dev = NULL;
    }
}

/***********************************************************************
 * Transaction handler
 **********************************************************************/
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

/***********************************************************************
 * Handler dispatcher implementation
 **********************************************************************/
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

        packer & SoapySDR::Device::enumerate(args);

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

        std::lock_guard<std::mutex> lock(factoryMutex);
        _dev = SoapySDR::Device::make(args);
        packer & SOAPY_REMOTE_VOID;
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_UNMAKE:
    ////////////////////////////////////////////////////////////////////
    {
        std::lock_guard<std::mutex> lock(factoryMutex);
        if (_dev != NULL) SoapySDR::Device::unmake(_dev);
        _dev = NULL;
        packer & SOAPY_REMOTE_VOID;

        //client is done with the socket
        return false;
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_GET_DRIVER_KEY:
    ////////////////////////////////////////////////////////////////////
    {
        packer & _dev->getDriverKey();
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_GET_HARDWARE_KEY:
    ////////////////////////////////////////////////////////////////////
    {
        packer & _dev->getHardwareKey();
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_GET_HARDWARE_INFO:
    ////////////////////////////////////////////////////////////////////
    {
        packer & _dev->getHardwareInfo();
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_SET_FRONTEND_MAPPING:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        std::string mapping;
        unpacker & direction;
        unpacker & mapping;
        _dev->setFrontendMapping(direction, mapping);
        packer & SOAPY_REMOTE_VOID;
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_GET_FRONTEND_MAPPING:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        unpacker & direction;
        packer & _dev->getFrontendMapping(direction);
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_GET_NUM_CHANNELS:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        unpacker & direction;
        packer & int(_dev->getNumChannels(direction));
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_GET_FULL_DUPLEX:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        unpacker & direction;
        unpacker & channel;
        packer & _dev->getFullDuplex(direction, channel);
    } break;

    default: break; //TODO unknown...
    }

    return true;
}
