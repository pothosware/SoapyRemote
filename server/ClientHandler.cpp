// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "ClientHandler.hpp"
#include "ServerStreamData.hpp"
#include "LogForwarding.hpp"
#include "SoapyRemoteDefs.hpp"
#include "FormatToElemSize.hpp"
#include "SoapyURLUtils.hpp"
#include "SoapyRPCSocket.hpp"
#include "SoapyRPCPacker.hpp"
#include "SoapyRPCUnpacker.hpp"
#include "SoapyStreamEndpoint.hpp"
#include <SoapySDR/Device.hpp>
#include <SoapySDR/Logger.hpp>
#include <iostream>
#include <mutex>

//! The device factory make and unmake requires a process-wide mutex
static std::mutex factoryMutex;

/***********************************************************************
 * Args translator for nested keywords
 **********************************************************************/
static void translateArgs(SoapySDR::Kwargs &args)
{
    //stop infinite loops with special keyword
    args[SOAPY_REMOTE_KWARG_STOP] = "";

    //rewrite driver from remoteDriver
    args.erase("driver");
    if (args.count("remoteDriver") != 0)
    {
        args["driver"] = args["remoteDriver"];
        args.erase("remoteDriver");
    }
}

/***********************************************************************
 * Client handler constructor
 **********************************************************************/
SoapyClientHandler::SoapyClientHandler(SoapyRPCSocket &sock):
    _sock(sock),
    _dev(nullptr),
    _logForwarder(nullptr),
    _nextStreamId(0)
{
    return;
}

SoapyClientHandler::~SoapyClientHandler(void)
{
    if (_dev != nullptr)
    {
        std::lock_guard<std::mutex> lock(factoryMutex);
        SoapySDR::Device::unmake(_dev);
        _dev = nullptr;
    }
    delete _logForwarder;
}

/***********************************************************************
 * Transaction handler
 **********************************************************************/
bool SoapyClientHandler::handleOnce(void)
{
    if (not _sock.selectRecv(SOAPY_REMOTE_SOCKET_TIMEOUT_US)) return true;

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
        translateArgs(args);
        packer & SoapySDR::Device::enumerate(args);
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_MAKE:
    ////////////////////////////////////////////////////////////////////
    {
        SoapySDR::Kwargs args;
        unpacker & args;
        translateArgs(args);
        std::lock_guard<std::mutex> lock(factoryMutex);
        if (_dev == nullptr) _dev = SoapySDR::Device::make(args);
        packer & SOAPY_REMOTE_VOID;
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_UNMAKE:
    ////////////////////////////////////////////////////////////////////
    {
        std::lock_guard<std::mutex> lock(factoryMutex);
        if (_dev != nullptr) SoapySDR::Device::unmake(_dev);
        _dev = nullptr;
        packer & SOAPY_REMOTE_VOID;
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_HANGUP:
    ////////////////////////////////////////////////////////////////////
    {
        packer & SOAPY_REMOTE_VOID;
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_GET_SERVER_ID:
    ////////////////////////////////////////////////////////////////////
    {
        packer & uniqueProcessId();
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_START_LOG_FORWARDING:
    ////////////////////////////////////////////////////////////////////
    {
        if (_logForwarder == nullptr) _logForwarder = new SoapyLogForwarder(_sock);
        packer & SOAPY_REMOTE_VOID;
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_STOP_LOG_FORWARDING:
    ////////////////////////////////////////////////////////////////////
    {
        if (_logForwarder != nullptr) delete _logForwarder;
        _logForwarder = nullptr;
        packer & SOAPY_REMOTE_VOID;
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

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_SETUP_STREAM:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        std::string format;
        std::vector<size_t> channels;
        SoapySDR::Kwargs args;
        std::string clientBindPort;
        std::string statusBindPort;
        unpacker & direction;
        unpacker & format;
        unpacker & channels;
        unpacker & args;
        unpacker & clientBindPort;
        unpacker & statusBindPort;

        //parse args for buffer configuration
        size_t mtu = SOAPY_REMOTE_DEFAULT_ENDPOINT_MTU;
        if (args.count("remoteMTU") != 0) mtu = std::stoul(args.at("remoteMTU"));

        size_t window = SOAPY_REMOTE_DEFAULT_ENDPOINT_WINDOW;
        if (args.count("remoteWindow") != 0) window = std::stoul(args.at("remoteWindow"));

        //create stream
        auto stream = _dev->setupStream(direction, format, channels, args);

        //load data structure
        auto &data = _streamData[_nextStreamId];
        data.streamId = _nextStreamId++;
        data.device = _dev;
        data.stream = stream;
        data.format = format;

        //bind the stream socket to an automatic port
        std::string serverBindPort;
        std::string scheme, node, service;
        splitURL(_sock.getsockname(), scheme, node, service);
        const auto bindURL = combineURL("udp", node, "0");
        int ret = data.streamSock.bind(bindURL);
        if (ret != 0)
        {
            const std::string errorMsg = data.streamSock.lastErrorMsg();
            _streamData.erase(data.streamId);
            throw std::runtime_error("SoapyRemote::setupStream("+bindURL+") -- bind FAIL: " + errorMsg);
        }
        SoapySDR::logf(SOAPY_SDR_INFO, "Server side stream bound to %s", data.streamSock.getsockname().c_str());
        splitURL(data.streamSock.getsockname(), scheme, node, serverBindPort);

        //connect the stream socket to the specified port
        splitURL(_sock.getpeername(), scheme, node, service);
        auto connectURL = combineURL("udp", node, clientBindPort);
        ret = data.streamSock.connect(connectURL);
        if (ret != 0)
        {
            const std::string errorMsg = data.streamSock.lastErrorMsg();
            _streamData.erase(data.streamId);
            throw std::runtime_error("SoapyRemote::setupStream("+connectURL+") -- connect FAIL: " + errorMsg);
        }
        SoapySDR::logf(SOAPY_SDR_INFO, "Server side stream connected to %s", data.streamSock.getpeername().c_str());

        //connect the status socket to the specified port
        connectURL = combineURL("udp", node, statusBindPort);
        ret = data.statusSock.connect(connectURL);
        if (ret != 0)
        {
            const std::string errorMsg = data.statusSock.lastErrorMsg();
            _streamData.erase(data.streamId);
            throw std::runtime_error("SoapyRemote::setupStream("+connectURL+") -- connect FAIL: " + errorMsg);
        }
        SoapySDR::logf(SOAPY_SDR_INFO, "Server side status connected to %s", data.statusSock.getpeername().c_str());

        //create endpoint
        data.endpoint = new SoapyStreamEndpoint(data.streamSock, data.statusSock,
            direction == SOAPY_SDR_TX, channels.size(), formatToSize(format), mtu, window);

        //start worker thread, this is not backwards,
        //receive from device means using a send endpoint
        //transmit to device means using a recv endpoint
        if (direction == SOAPY_SDR_RX) data.startSendThread();
        if (direction == SOAPY_SDR_TX) data.startRecvThread();
        data.startStatThread();

        packer & data.streamId;
        packer & serverBindPort;
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_CLOSE_STREAM:
    ////////////////////////////////////////////////////////////////////
    {
        int streamId = 0;
        unpacker & streamId;

        //cleanup data and stop worker thread
        auto &data = _streamData[streamId];
        data.stopThreads();
        _streamData.erase(streamId);

        packer & SOAPY_REMOTE_VOID;
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_ACTIVATE_STREAM:
    ////////////////////////////////////////////////////////////////////
    {
        int streamId = 0;
        int flags = 0;
        long long timeNs = 0;
        int numElems = 0;
        unpacker & streamId;
        unpacker & flags;
        unpacker & timeNs;
        unpacker & numElems;

        auto &data = _streamData[streamId];
        packer & _dev->activateStream(data.stream, flags, timeNs, size_t(numElems));
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_DEACTIVATE_STREAM:
    ////////////////////////////////////////////////////////////////////
    {
        int streamId = 0;
        int flags = 0;
        long long timeNs = 0;
        unpacker & streamId;
        unpacker & flags;
        unpacker & timeNs;

        auto &data = _streamData[streamId];
        packer & _dev->deactivateStream(data.stream, flags, timeNs);
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_LIST_ANTENNAS:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        unpacker & direction;
        unpacker & channel;
        packer & _dev->listAntennas(direction, channel);
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_SET_ANTENNA:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        std::string name = 0;
        unpacker & direction;
        unpacker & channel;
        unpacker & name;
        _dev->setAntenna(direction, channel, name);
        packer & SOAPY_REMOTE_VOID;
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_GET_ANTENNA:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        unpacker & direction;
        unpacker & channel;
        packer & _dev->getAntenna(direction, channel);
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_HAS_DC_OFFSET_MODE:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        unpacker & direction;
        unpacker & channel;
        packer & _dev->hasDCOffsetMode(direction, channel);
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_SET_DC_OFFSET_MODE:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        bool automatic = false;
        unpacker & direction;
        unpacker & channel;
        unpacker & automatic;
        _dev->setDCOffsetMode(direction, channel, automatic);
        packer & SOAPY_REMOTE_VOID;
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_GET_DC_OFFSET_MODE:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        unpacker & direction;
        unpacker & channel;
        packer & _dev->getDCOffsetMode(direction, channel);
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_HAS_DC_OFFSET:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        unpacker & direction;
        unpacker & channel;
        packer & _dev->hasDCOffset(direction, channel);
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_SET_DC_OFFSET:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        std::complex<double> offset;
        unpacker & direction;
        unpacker & channel;
        unpacker & offset;
        _dev->setDCOffset(direction, channel, offset);
        packer & SOAPY_REMOTE_VOID;
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_GET_DC_OFFSET:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        unpacker & direction;
        unpacker & channel;
        packer & _dev->getDCOffset(direction, channel);
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_HAS_IQ_BALANCE_MODE:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        unpacker & direction;
        unpacker & channel;
        packer & _dev->hasIQBalance(direction, channel);
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_SET_IQ_BALANCE_MODE:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        std::complex<double> balance;
        unpacker & direction;
        unpacker & channel;
        unpacker & balance;
        _dev->setIQBalance(direction, channel, balance);
        packer & SOAPY_REMOTE_VOID;
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_GET_IQ_BALANCE_MODE:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        unpacker & direction;
        unpacker & channel;
        packer & _dev->getIQBalance(direction, channel);
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_LIST_GAINS:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        unpacker & direction;
        unpacker & channel;
        packer & _dev->listGains(direction, channel);
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_SET_GAIN_MODE:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        bool automatic = false;
        unpacker & direction;
        unpacker & channel;
        unpacker & automatic;
        _dev->setGainMode(direction, channel, automatic);
        packer & SOAPY_REMOTE_VOID;
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_GET_GAIN_MODE:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        unpacker & direction;
        unpacker & channel;
        packer & _dev->getGainMode(direction, channel);
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_SET_GAIN:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        double value = 0;
        unpacker & direction;
        unpacker & channel;
        unpacker & value;
        _dev->setGain(direction, channel, value);
        packer & SOAPY_REMOTE_VOID;
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_SET_GAIN_ELEMENT:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        std::string name;
        double value = 0;
        unpacker & direction;
        unpacker & channel;
        unpacker & name;
        unpacker & value;
        _dev->setGain(direction, channel, name, value);
        packer & SOAPY_REMOTE_VOID;
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_GET_GAIN:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        unpacker & direction;
        unpacker & channel;
        packer & _dev->getGain(direction, channel);
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_GET_GAIN_ELEMENT:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        std::string name;
        unpacker & direction;
        unpacker & channel;
        unpacker & name;
        packer & _dev->getGain(direction, channel, name);
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_GET_GAIN_RANGE:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        unpacker & direction;
        unpacker & channel;
        packer & _dev->getGainRange(direction, channel);
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_GET_GAIN_RANGE_ELEMENT:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        std::string name;
        unpacker & direction;
        unpacker & channel;
        unpacker & name;
        packer & _dev->getGainRange(direction, channel, name);
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_SET_FREQUENCY:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        double value = 0;
        SoapySDR::Kwargs args;
        unpacker & direction;
        unpacker & channel;
        unpacker & value;
        unpacker & args;
        _dev->setFrequency(direction, channel, value, args);
        packer & SOAPY_REMOTE_VOID;
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_SET_FREQUENCY_COMPONENT:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        std::string name;
        double value = 0;
        SoapySDR::Kwargs args;
        unpacker & direction;
        unpacker & channel;
        unpacker & name;
        unpacker & value;
        unpacker & args;
        _dev->setFrequency(direction, channel, name, value, args);
        packer & SOAPY_REMOTE_VOID;
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_GET_FREQUENCY:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        unpacker & direction;
        unpacker & channel;
        packer & _dev->getFrequency(direction, channel);
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_GET_FREQUENCY_COMPONENT:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        std::string name;
        unpacker & direction;
        unpacker & channel;
        unpacker & name;
        packer & _dev->getFrequency(direction, channel, name);
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_LIST_FREQUENCIES:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        unpacker & direction;
        unpacker & channel;
        packer & _dev->listFrequencies(direction, channel);
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_GET_FREQUENCY_RANGE:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        unpacker & direction;
        unpacker & channel;
        packer & _dev->getFrequencyRange(direction, channel);
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_GET_FREQUENCY_RANGE_COMPONENT:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        std::string name;
        unpacker & direction;
        unpacker & channel;
        unpacker & name;
        packer & _dev->getFrequencyRange(direction, channel, name);
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_SET_SAMPLE_RATE:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        double rate = 0;
        unpacker & direction;
        unpacker & channel;
        unpacker & rate;
        _dev->setSampleRate(direction, channel, rate);
        packer & SOAPY_REMOTE_VOID;
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_GET_SAMPLE_RATE:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        unpacker & direction;
        unpacker & channel;
        packer & _dev->getSampleRate(direction, channel);
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_LIST_SAMPLE_RATES:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        unpacker & direction;
        unpacker & channel;
        packer & _dev->listSampleRates(direction, channel);
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_SET_BANDWIDTH:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        double bw = 0;
        unpacker & direction;
        unpacker & channel;
        unpacker & bw;
        _dev->setBandwidth(direction, channel, bw);
        packer & SOAPY_REMOTE_VOID;
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_GET_BANDWIDTH:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        unpacker & direction;
        unpacker & channel;
        packer & _dev->getBandwidth(direction, channel);
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_LIST_BANDWIDTHS:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        unpacker & direction;
        unpacker & channel;
        packer & _dev->listBandwidths(direction, channel);
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_SET_MASTER_CLOCK_RATE:
    ////////////////////////////////////////////////////////////////////
    {
        double rate = 0;
        unpacker & rate;
        _dev->setMasterClockRate(rate);
        packer & SOAPY_REMOTE_VOID;
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_GET_MASTER_CLOCK_RATE:
    ////////////////////////////////////////////////////////////////////
    {
        packer & _dev->getMasterClockRate();
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_LIST_CLOCK_SOURCES:
    ////////////////////////////////////////////////////////////////////
    {
        packer & _dev->listClockSources();
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_SET_CLOCK_SOURCE:
    ////////////////////////////////////////////////////////////////////
    {
        std::string source;
        unpacker & source;
        _dev->setClockSource(source);
        packer & SOAPY_REMOTE_VOID;
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_GET_CLOCK_SOURCE:
    ////////////////////////////////////////////////////////////////////
    {
        packer & _dev->getClockSource();
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_LIST_TIME_SOURCES:
    ////////////////////////////////////////////////////////////////////
    {
        packer & _dev->listTimeSources();
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_SET_TIME_SOURCE:
    ////////////////////////////////////////////////////////////////////
    {
        std::string source;
        unpacker & source;
        _dev->setTimeSource(source);
        packer & SOAPY_REMOTE_VOID;
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_GET_TIME_SOURCE:
    ////////////////////////////////////////////////////////////////////
    {
        packer & _dev->getTimeSource();
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_HAS_HARDWARE_TIME:
    ////////////////////////////////////////////////////////////////////
    {
        std::string what;
        unpacker & what;
        packer & _dev->hasHardwareTime(what);
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_GET_HARDWARE_TIME:
    ////////////////////////////////////////////////////////////////////
    {
        std::string what;
        unpacker & what;
        packer & _dev->getHardwareTime(what);
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_SET_HARDWARE_TIME:
    ////////////////////////////////////////////////////////////////////
    {
        long long timeNs = 0;
        std::string what;
        unpacker & timeNs;
        unpacker & what;
        _dev->setHardwareTime(timeNs, what);
        packer & SOAPY_REMOTE_VOID;
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_SET_COMMAND_TIME:
    ////////////////////////////////////////////////////////////////////
    {
        long long timeNs = 0;
        std::string what;
        unpacker & timeNs;
        unpacker & what;
        _dev->setCommandTime(timeNs, what);
        packer & SOAPY_REMOTE_VOID;
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_LIST_SENSORS:
    ////////////////////////////////////////////////////////////////////
    {
        packer & _dev->listSensors();
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_READ_SENSOR:
    ////////////////////////////////////////////////////////////////////
    {
        std::string name;
        unpacker & name;
        packer & _dev->readSensor(name);
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_LIST_CHANNEL_SENSORS:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        unpacker & direction;
        unpacker & channel;
        packer & _dev->listSensors(direction, channel);
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_READ_CHANNEL_SENSOR:
    ////////////////////////////////////////////////////////////////////
    {
        char direction = 0;
        int channel = 0;
        std::string name;
        unpacker & direction;
        unpacker & channel;
        unpacker & name;
        packer & _dev->readSensor(direction, channel, name);
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_WRITE_REGISTER:
    ////////////////////////////////////////////////////////////////////
    {
        int addr = 0;
        int value = 0;
        unpacker & addr;
        unpacker & value;
        _dev->writeRegister(unsigned(addr), unsigned(value));
        packer & SOAPY_REMOTE_VOID;
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_READ_REGISTER:
    ////////////////////////////////////////////////////////////////////
    {
        int addr = 0;
        unpacker & addr;
        packer & int(_dev->readRegister(unsigned(addr)));
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_WRITE_SETTING:
    ////////////////////////////////////////////////////////////////////
    {
        std::string key;
        std::string value;
        unpacker & key;
        unpacker & value;
        _dev->writeSetting(key, value);
        packer & SOAPY_REMOTE_VOID;
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_READ_SETTING:
    ////////////////////////////////////////////////////////////////////
    {
        std::string key;
        unpacker & key;
        packer & _dev->readSetting(key);
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_LIST_GPIO_BANKS:
    ////////////////////////////////////////////////////////////////////
    {
        packer & _dev->listGPIOBanks();
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_WRITE_GPIO:
    ////////////////////////////////////////////////////////////////////
    {
        std::string bank;
        int value = 0;
        unpacker & bank;
        unpacker & value;
        _dev->writeGPIO(bank, unsigned(value));
        packer & SOAPY_REMOTE_VOID;
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_WRITE_GPIO_MASKED:
    ////////////////////////////////////////////////////////////////////
    {
        std::string bank;
        int value = 0;
        int mask = 0;
        unpacker & bank;
        unpacker & value;
        unpacker & mask;
        _dev->writeGPIO(bank, unsigned(value), unsigned(mask));
        packer & SOAPY_REMOTE_VOID;
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_READ_GPIO:
    ////////////////////////////////////////////////////////////////////
    {
        std::string bank;
        unpacker & bank;
        packer & int(_dev->readGPIO(bank));
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_WRITE_GPIO_DIR:
    ////////////////////////////////////////////////////////////////////
    {
        std::string bank;
        int dir = 0;
        unpacker & bank;
        unpacker & dir;
        _dev->writeGPIODir(bank, unsigned(dir));
        packer & SOAPY_REMOTE_VOID;
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_WRITE_GPIO_DIR_MASKED:
    ////////////////////////////////////////////////////////////////////
    {
        std::string bank;
        int dir = 0;
        int mask = 0;
        unpacker & bank;
        unpacker & dir;
        unpacker & mask;
        _dev->writeGPIODir(bank, unsigned(dir), unsigned(mask));
        packer & SOAPY_REMOTE_VOID;
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_READ_GPIO_DIR:
    ////////////////////////////////////////////////////////////////////
    {
        std::string bank;
        unpacker & bank;
        packer & int(_dev->readGPIODir(bank));
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_WRITE_I2C:
    ////////////////////////////////////////////////////////////////////
    {
        int addr = 0;
        std::string data;
        unpacker & addr;
        unpacker & data;
        _dev->writeI2C(addr, data);
        packer & SOAPY_REMOTE_VOID;
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_READ_I2C:
    ////////////////////////////////////////////////////////////////////
    {
        int addr = 0;
        int numBytes = 0;
        unpacker & addr;
        unpacker & numBytes;
        packer & _dev->readI2C(addr, unsigned(numBytes));
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_TRANSACT_SPI:
    ////////////////////////////////////////////////////////////////////
    {
        int addr = 0;
        int data = 0;
        int numBits = 0;
        unpacker & addr;
        unpacker & data;
        unpacker & numBits;
        packer & int(_dev->transactSPI(addr, unsigned(data), size_t(numBits)));
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_LIST_UARTS:
    ////////////////////////////////////////////////////////////////////
    {
        packer & _dev->listUARTs();
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_WRITE_UART:
    ////////////////////////////////////////////////////////////////////
    {
        std::string which;
        std::string data;
        unpacker & which;
        unpacker & data;
        _dev->writeUART(which, data);
        packer & SOAPY_REMOTE_VOID;
    } break;

    ////////////////////////////////////////////////////////////////////
    case SOAPY_REMOTE_READ_UART:
    ////////////////////////////////////////////////////////////////////
    {
        std::string which;
        int timeoutUs = 0;
        unpacker & which;
        unpacker & timeoutUs;
        packer & _dev->readUART(which, long(timeoutUs));
    } break;

    default: throw std::runtime_error(
        "SoapyClientHandler::handleOnce("+std::to_string(int(call))+") unknown call");
    }

    return call != SOAPY_REMOTE_HANGUP;
}
