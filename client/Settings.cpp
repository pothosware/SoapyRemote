// Copyright (c) 2015-2020 Josh Blum
// Copyright (c) 2016-2016 Bastille Networks
// SPDX-License-Identifier: BSL-1.0

#include "SoapyClient.hpp"
#include "LogAcceptor.hpp"
#include "SoapyRemoteDefs.hpp"
#include "SoapyRPCPacker.hpp"
#include "SoapyRPCUnpacker.hpp"
#include <SoapySDR/Logger.hpp>
#include <stdexcept>

/*******************************************************************
 * Constructor
 ******************************************************************/

SoapyRemoteDevice::SoapyRemoteDevice(const std::string &url, const SoapySDR::Kwargs &args):
    _logAcceptor(nullptr),
    _defaultStreamProt("udp")
{
    //extract timeout
    long timeoutUs = SOAPY_REMOTE_SOCKET_TIMEOUT_US;
    const auto timeoutIt = args.find("timeout");
    if (timeoutIt != args.end()) timeoutUs = std::stol(timeoutIt->second);

    //try to connect to the remote server
    int ret = _sock.connect(url, timeoutUs);
    if (ret != 0)
    {
        throw std::runtime_error("SoapyRemoteDevice("+url+") -- connect FAIL: " + _sock.lastErrorMsg());
    }

    //connect the log acceptor
    _logAcceptor = new SoapyLogAcceptor(url, _sock, timeoutUs);

    //acquire device instance
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_MAKE;
    packer & args;
    packer();
    SoapyRPCUnpacker unpacker(_sock);

    //default stream protocol specified in device args
    const auto protIt = args.find("prot");
    if (protIt != args.end()) _defaultStreamProt = protIt->second;
}

SoapyRemoteDevice::~SoapyRemoteDevice(void)
{
    //cant throw in the destructor
    try
    {
        //release device instance
        SoapyRPCPacker packer(_sock);
        packer & SOAPY_REMOTE_UNMAKE;
        packer();
        SoapyRPCUnpacker unpacker(_sock);

        //graceful disconnect
        SoapyRPCPacker packerHangup(_sock);
        packerHangup & SOAPY_REMOTE_HANGUP;
        packerHangup();
        SoapyRPCUnpacker unpackerHangup(_sock);
    }
    catch (const std::exception &ex)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "~SoapyRemoteDevice() FAIL: %s", ex.what());
    }

    //disconnect the log acceptor (does not throw)
    delete _logAcceptor;
}

/*******************************************************************
 * Identification API
 ******************************************************************/

std::string SoapyRemoteDevice::getDriverKey(void) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_DRIVER_KEY;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    std::string result;
    unpacker & result;
    return result;
}

std::string SoapyRemoteDevice::getHardwareKey(void) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_HARDWARE_KEY;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    std::string result;
    unpacker & result;
    return result;
}

SoapySDR::Kwargs SoapyRemoteDevice::getHardwareInfo(void) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_HARDWARE_INFO;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    SoapySDR::Kwargs result;
    unpacker & result;
    return result;
}

/*******************************************************************
 * Channels API
 ******************************************************************/

void SoapyRemoteDevice::setFrontendMapping(const int direction, const std::string &mapping)
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_SET_FRONTEND_MAPPING;
    packer & char(direction);
    packer & mapping;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
}

std::string SoapyRemoteDevice::getFrontendMapping(const int direction) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_FRONTEND_MAPPING;
    packer & char(direction);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    std::string result;
    unpacker & result;
    return result;
}

size_t SoapyRemoteDevice::getNumChannels(const int direction) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_NUM_CHANNELS;
    packer & char(direction);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    int result;
    unpacker & result;
    return result;
}

SoapySDR::Kwargs SoapyRemoteDevice::getChannelInfo(const int direction, const size_t channel) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_CHANNEL_INFO;
    packer & char(direction);
    packer & int(channel);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    SoapySDR::Kwargs result;
    unpacker & result;
    return result;
}

bool SoapyRemoteDevice::getFullDuplex(const int direction, const size_t channel) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_FULL_DUPLEX;
    packer & char(direction);
    packer & int(channel);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    bool result;
    unpacker & result;
    return result;
}

/*******************************************************************
 * Antenna API
 ******************************************************************/

std::vector<std::string> SoapyRemoteDevice::listAntennas(const int direction, const size_t channel) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_LIST_ANTENNAS;
    packer & char(direction);
    packer & int(channel);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    std::vector<std::string> result;
    unpacker & result;
    return result;
}

void SoapyRemoteDevice::setAntenna(const int direction, const size_t channel, const std::string &name)
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_SET_ANTENNA;
    packer & char(direction);
    packer & int(channel);
    packer & name;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
}

std::string SoapyRemoteDevice::getAntenna(const int direction, const size_t channel) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_ANTENNA;
    packer & char(direction);
    packer & int(channel);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    std::string result;
    unpacker & result;
    return result;
}

/*******************************************************************
 * Frontend corrections API
 ******************************************************************/

bool SoapyRemoteDevice::hasDCOffsetMode(const int direction, const size_t channel) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_HAS_DC_OFFSET_MODE;
    packer & char(direction);
    packer & int(channel);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    bool result;
    unpacker & result;
    return result;
}

void SoapyRemoteDevice::setDCOffsetMode(const int direction, const size_t channel, const bool automatic)
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_SET_DC_OFFSET_MODE;
    packer & char(direction);
    packer & int(channel);
    packer & automatic;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
}

bool SoapyRemoteDevice::getDCOffsetMode(const int direction, const size_t channel) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_DC_OFFSET_MODE;
    packer & char(direction);
    packer & int(channel);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    bool result;
    unpacker & result;
    return result;
}

bool SoapyRemoteDevice::hasDCOffset(const int direction, const size_t channel) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_HAS_DC_OFFSET;
    packer & char(direction);
    packer & int(channel);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    bool result;
    unpacker & result;
    return result;
}

void SoapyRemoteDevice::setDCOffset(const int direction, const size_t channel, const std::complex<double> &offset)
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_SET_DC_OFFSET;
    packer & char(direction);
    packer & int(channel);
    packer & offset;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
}

std::complex<double> SoapyRemoteDevice::getDCOffset(const int direction, const size_t channel) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_DC_OFFSET;
    packer & char(direction);
    packer & int(channel);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    std::complex<double> result;
    unpacker & result;
    return result;
}

bool SoapyRemoteDevice::hasIQBalance(const int direction, const size_t channel) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_HAS_IQ_BALANCE_MODE;
    packer & char(direction);
    packer & int(channel);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    bool result;
    unpacker & result;
    return result;
}

void SoapyRemoteDevice::setIQBalance(const int direction, const size_t channel, const std::complex<double> &balance)
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_SET_IQ_BALANCE_MODE;
    packer & char(direction);
    packer & int(channel);
    packer & balance;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
}

std::complex<double> SoapyRemoteDevice::getIQBalance(const int direction, const size_t channel) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_IQ_BALANCE_MODE;
    packer & char(direction);
    packer & int(channel);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    std::complex<double> result;
    unpacker & result;
    return result;
}

bool SoapyRemoteDevice::hasFrequencyCorrection(const int direction, const size_t channel) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_HAS_FREQUENCY_CORRECTION;
    packer & char(direction);
    packer & int(channel);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    bool result;
    unpacker & result;
    return result;
}

bool SoapyRemoteDevice::hasIQBalanceMode(const int direction, const size_t channel) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_HAS_IQ_BALANCE_MODE_AUTO;
    packer & char(direction);
    packer & int(channel);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    bool result;
    unpacker & result;
    return result;
}

void SoapyRemoteDevice::setIQBalanceMode(const int direction, const size_t channel, const bool automatic)
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_SET_IQ_BALANCE_MODE_AUTO;
    packer & char(direction);
    packer & int(channel);
    packer & automatic;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
}

bool SoapyRemoteDevice::getIQBalanceMode(const int direction, const size_t channel) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_IQ_BALANCE_MODE_AUTO;
    packer & char(direction);
    packer & int(channel);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    bool result;
    unpacker & result;
    return result;
}

void SoapyRemoteDevice::setFrequencyCorrection(const int direction, const size_t channel, const double value)
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_SET_FREQUENCY_CORRECTION;
    packer & char(direction);
    packer & int(channel);
    packer & value;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
}

double SoapyRemoteDevice::getFrequencyCorrection(const int direction, const size_t channel) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_FREQUENCY_CORRECTION;
    packer & char(direction);
    packer & int(channel);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    double result;
    unpacker & result;
    return result;
}

/*******************************************************************
 * Gain API
 ******************************************************************/

std::vector<std::string> SoapyRemoteDevice::listGains(const int direction, const size_t channel) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_LIST_GAINS;
    packer & char(direction);
    packer & int(channel);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    std::vector<std::string> result;
    unpacker & result;
    return result;
}

bool SoapyRemoteDevice::hasGainMode(const int direction, const size_t channel) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_HAS_GAIN_MODE;
    packer & char(direction);
    packer & int(channel);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    bool result;
    unpacker & result;
    return result;
}

void SoapyRemoteDevice::setGainMode(const int direction, const size_t channel, const bool automatic)
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_SET_GAIN_MODE;
    packer & char(direction);
    packer & int(channel);
    packer & automatic;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
}

bool SoapyRemoteDevice::getGainMode(const int direction, const size_t channel) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_GAIN_MODE;
    packer & char(direction);
    packer & int(channel);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    bool result;
    unpacker & result;
    return result;
}

void SoapyRemoteDevice::setGain(const int direction, const size_t channel, const double value)
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_SET_GAIN;
    packer & char(direction);
    packer & int(channel);
    packer & value;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
}

void SoapyRemoteDevice::setGain(const int direction, const size_t channel, const std::string &name, const double value)
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_SET_GAIN_ELEMENT;
    packer & char(direction);
    packer & int(channel);
    packer & name;
    packer & value;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
}

double SoapyRemoteDevice::getGain(const int direction, const size_t channel) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_GAIN;
    packer & char(direction);
    packer & int(channel);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    double result;
    unpacker & result;
    return result;
}

double SoapyRemoteDevice::getGain(const int direction, const size_t channel, const std::string &name) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_GAIN_ELEMENT;
    packer & char(direction);
    packer & int(channel);
    packer & name;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    double result;
    unpacker & result;
    return result;
}

SoapySDR::Range SoapyRemoteDevice::getGainRange(const int direction, const size_t channel) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_GAIN_RANGE;
    packer & char(direction);
    packer & int(channel);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    SoapySDR::Range result;
    unpacker & result;
    return result;
}

SoapySDR::Range SoapyRemoteDevice::getGainRange(const int direction, const size_t channel, const std::string &name) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_GAIN_RANGE_ELEMENT;
    packer & char(direction);
    packer & int(channel);
    packer & name;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    SoapySDR::Range result;
    unpacker & result;
    return result;
}

/*******************************************************************
 * Frequency API
 ******************************************************************/

void SoapyRemoteDevice::setFrequency(const int direction, const size_t channel, const double frequency, const SoapySDR::Kwargs &args)
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_SET_FREQUENCY;
    packer & char(direction);
    packer & int(channel);
    packer & frequency;
    packer & args;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
}

void SoapyRemoteDevice::setFrequency(const int direction, const size_t channel, const std::string &name, const double frequency, const SoapySDR::Kwargs &args)
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_SET_FREQUENCY_COMPONENT;
    packer & char(direction);
    packer & int(channel);
    packer & name;
    packer & frequency;
    packer & args;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
}

double SoapyRemoteDevice::getFrequency(const int direction, const size_t channel) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_FREQUENCY;
    packer & char(direction);
    packer & int(channel);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    double result;
    unpacker & result;
    return result;
}

double SoapyRemoteDevice::getFrequency(const int direction, const size_t channel, const std::string &name) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_FREQUENCY_COMPONENT;
    packer & char(direction);
    packer & int(channel);
    packer & name;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    double result;
    unpacker & result;
    return result;
}

std::vector<std::string> SoapyRemoteDevice::listFrequencies(const int direction, const size_t channel) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_LIST_FREQUENCIES;
    packer & char(direction);
    packer & int(channel);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    std::vector<std::string> result;
    unpacker & result;
    return result;
}

SoapySDR::RangeList SoapyRemoteDevice::getFrequencyRange(const int direction, const size_t channel) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_FREQUENCY_RANGE;
    packer & char(direction);
    packer & int(channel);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    SoapySDR::RangeList result;
    unpacker & result;
    return result;
}

SoapySDR::RangeList SoapyRemoteDevice::getFrequencyRange(const int direction, const size_t channel, const std::string &name) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_FREQUENCY_RANGE_COMPONENT;
    packer & char(direction);
    packer & int(channel);
    packer & name;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    SoapySDR::RangeList result;
    unpacker & result;
    return result;
}

SoapySDR::ArgInfoList SoapyRemoteDevice::getFrequencyArgsInfo(const int direction, const size_t channel) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_FREQUENCY_ARGS_INFO;
    packer & char(direction);
    packer & int(channel);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    SoapySDR::ArgInfoList result;
    unpacker & result;
    return result;
}

/*******************************************************************
 * Sample Rate API
 ******************************************************************/

void SoapyRemoteDevice::setSampleRate(const int direction, const size_t channel, const double rate)
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_SET_SAMPLE_RATE;
    packer & char(direction);
    packer & int(channel);
    packer & rate;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
}

double SoapyRemoteDevice::getSampleRate(const int direction, const size_t channel) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_SAMPLE_RATE;
    packer & char(direction);
    packer & int(channel);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    double result;
    unpacker & result;
    return result;
}

std::vector<double> SoapyRemoteDevice::listSampleRates(const int direction, const size_t channel) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_LIST_SAMPLE_RATES;
    packer & char(direction);
    packer & int(channel);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    std::vector<double> result;
    unpacker & result;
    return result;
}

SoapySDR::RangeList SoapyRemoteDevice::getSampleRateRange(const int direction, const size_t channel) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_SAMPLE_RATE_RANGE;
    packer & char(direction);
    packer & int(channel);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    SoapySDR::RangeList result;
    unpacker & result;
    return result;
}

/*******************************************************************
 * Bandwidth API
 ******************************************************************/

void SoapyRemoteDevice::setBandwidth(const int direction, const size_t channel, const double bw)
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_SET_BANDWIDTH;
    packer & char(direction);
    packer & int(channel);
    packer & bw;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
}

double SoapyRemoteDevice::getBandwidth(const int direction, const size_t channel) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_BANDWIDTH;
    packer & char(direction);
    packer & int(channel);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    double result;
    unpacker & result;
    return result;
}

std::vector<double> SoapyRemoteDevice::listBandwidths(const int direction, const size_t channel) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_LIST_BANDWIDTHS;
    packer & char(direction);
    packer & int(channel);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    std::vector<double> result;
    unpacker & result;
    return result;
}

SoapySDR::RangeList SoapyRemoteDevice::getBandwidthRange(const int direction, const size_t channel) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_BANDWIDTH_RANGE;
    packer & char(direction);
    packer & int(channel);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    SoapySDR::RangeList result;
    unpacker & result;
    return result;
}

/*******************************************************************
 * Clocking API
 ******************************************************************/

void SoapyRemoteDevice::setMasterClockRate(const double rate)
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_SET_MASTER_CLOCK_RATE;
    packer & rate;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
}

double SoapyRemoteDevice::getMasterClockRate(void) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_MASTER_CLOCK_RATE;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    double result;
    unpacker & result;
    return result;
}

SoapySDR::RangeList SoapyRemoteDevice::getMasterClockRates(void) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_MASTER_CLOCK_RATES;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    SoapySDR::RangeList result;
    unpacker & result;
    return result;
}

void SoapyRemoteDevice::setReferenceClockRate(const double rate)
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_SET_REF_CLOCK_RATE;
    packer & rate;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
}

double SoapyRemoteDevice::getReferenceClockRate(void) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_REF_CLOCK_RATE;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    double result;
    unpacker & result;
    return result;
}

SoapySDR::RangeList SoapyRemoteDevice::getReferenceClockRates(void) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_REF_CLOCK_RATES;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    SoapySDR::RangeList result;
    unpacker & result;
    return result;
}

std::vector<std::string> SoapyRemoteDevice::listClockSources(void) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_LIST_CLOCK_SOURCES;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    std::vector<std::string> result;
    unpacker & result;
    return result;
}

void SoapyRemoteDevice::setClockSource(const std::string &source)
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_SET_CLOCK_SOURCE;
    packer & source;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
}

std::string SoapyRemoteDevice::getClockSource(void) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_CLOCK_SOURCE;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    std::string result;
    unpacker & result;
    return result;
}

/*******************************************************************
 * Time API
 ******************************************************************/

std::vector<std::string> SoapyRemoteDevice::listTimeSources(void) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_LIST_TIME_SOURCES;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    std::vector<std::string> result;
    unpacker & result;
    return result;
}

void SoapyRemoteDevice::setTimeSource(const std::string &source)
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_SET_TIME_SOURCE;
    packer & source;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
}

std::string SoapyRemoteDevice::getTimeSource(void) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_TIME_SOURCE;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    std::string result;
    unpacker & result;
    return result;
}

bool SoapyRemoteDevice::hasHardwareTime(const std::string &what) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_HAS_HARDWARE_TIME;
    packer & what;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    bool result;
    unpacker & result;
    return result;
}

long long SoapyRemoteDevice::getHardwareTime(const std::string &what) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_HARDWARE_TIME;
    packer & what;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    long long result;
    unpacker & result;
    return result;
}

void SoapyRemoteDevice::setHardwareTime(const long long timeNs, const std::string &what)
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_SET_HARDWARE_TIME;
    packer & timeNs;
    packer & what;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
}

void SoapyRemoteDevice::setCommandTime(const long long timeNs, const std::string &what)
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_SET_COMMAND_TIME;
    packer & timeNs;
    packer & what;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
}

/*******************************************************************
 * Sensor API
 ******************************************************************/

std::vector<std::string> SoapyRemoteDevice::listSensors(void) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_LIST_SENSORS;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    std::vector<std::string> result;
    unpacker & result;
    return result;
}

SoapySDR::ArgInfo SoapyRemoteDevice::getSensorInfo(const std::string &name) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_SENSOR_INFO;
    packer & name;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    SoapySDR::ArgInfo result;
    unpacker & result;
    return result;
}

std::string SoapyRemoteDevice::readSensor(const std::string &name) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_READ_SENSOR;
    packer & name;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    std::string result;
    unpacker & result;
    return result;
}

std::vector<std::string> SoapyRemoteDevice::listSensors(const int direction, const size_t channel) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_LIST_CHANNEL_SENSORS;
    packer & char(direction);
    packer & int(channel);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    std::vector<std::string> result;
    unpacker & result;
    return result;
}

SoapySDR::ArgInfo SoapyRemoteDevice::getSensorInfo(const int direction, const size_t channel, const std::string &name) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_CHANNEL_SENSOR_INFO;
    packer & char(direction);
    packer & int(channel);
    packer & name;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    SoapySDR::ArgInfo result;
    unpacker & result;
    return result;
}

std::string SoapyRemoteDevice::readSensor(const int direction, const size_t channel, const std::string &name) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_READ_CHANNEL_SENSOR;
    packer & char(direction);
    packer & int(channel);
    packer & name;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    std::string result;
    unpacker & result;
    return result;
}

/*******************************************************************
 * Register API
 ******************************************************************/

std::vector<std::string> SoapyRemoteDevice::listRegisterInterfaces(void) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_LIST_REGISTER_INTERFACES;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    std::vector<std::string> result;
    unpacker & result;
    return result;
}

void SoapyRemoteDevice::writeRegister(const std::string &name, const unsigned addr, const unsigned value)
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_WRITE_REGISTER_NAMED;
    packer & name;
    packer & int(addr);
    packer & int(value);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
}

unsigned SoapyRemoteDevice::readRegister(const std::string &name, const unsigned addr) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_READ_REGISTER_NAMED;
    packer & name;
    packer & int(addr);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    int result;
    unpacker & result;
    return unsigned(result);
}

void SoapyRemoteDevice::writeRegister(const unsigned addr, const unsigned value)
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_WRITE_REGISTER;
    packer & int(addr);
    packer & int(value);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
}

unsigned SoapyRemoteDevice::readRegister(const unsigned addr) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_READ_REGISTER;
    packer & int(addr);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    int result;
    unpacker & result;
    return unsigned(result);
}

void SoapyRemoteDevice::writeRegisters(const std::string &name, const unsigned addr, const std::vector<unsigned> &value)
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    std::vector<size_t> val (value.begin(), value.end());
    packer & SOAPY_REMOTE_WRITE_REGISTERS;
    packer & name;
    packer & int(addr);
    packer & val;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
}

std::vector<unsigned> SoapyRemoteDevice::readRegisters(const std::string &name, const unsigned addr, const size_t length) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_READ_REGISTERS;
    packer & name;
    packer & int(addr);
    packer & int(length);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    std::vector<size_t> result;
    unpacker & result;
    std::vector<unsigned> res (result.begin(), result.end());
    return res;
}

/*******************************************************************
 * Settings API
 ******************************************************************/

SoapySDR::ArgInfoList SoapyRemoteDevice::getSettingInfo(void) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_SETTING_INFO;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    SoapySDR::ArgInfoList result;
    unpacker & result;
    return result;
}

void SoapyRemoteDevice::writeSetting(const std::string &key, const std::string &value)
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_WRITE_SETTING;
    packer & key;
    packer & value;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
}

std::string SoapyRemoteDevice::readSetting(const std::string &key) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_READ_SETTING;
    packer & key;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    std::string result;
    unpacker & result;
    return result;
}

SoapySDR::ArgInfoList SoapyRemoteDevice::getSettingInfo(const int direction, const size_t channel) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_CHANNEL_SETTING_INFO;
    packer & char(direction);
    packer & int(channel);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    SoapySDR::ArgInfoList result;
    unpacker & result;
    return result;
}

void SoapyRemoteDevice::writeSetting(const int direction, const size_t channel, const std::string &key, const std::string &value)
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_WRITE_CHANNEL_SETTING;
    packer & char(direction);
    packer & int(channel);
    packer & key;
    packer & value;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
}

std::string SoapyRemoteDevice::readSetting(const int direction, const size_t channel, const std::string &key) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_READ_CHANNEL_SETTING;
    packer & char(direction);
    packer & int(channel);
    packer & key;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    std::string result;
    unpacker & result;
    return result;
}

/*******************************************************************
 * GPIO API
 ******************************************************************/

std::vector<std::string> SoapyRemoteDevice::listGPIOBanks(void) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_LIST_GPIO_BANKS;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    std::vector<std::string> result;
    unpacker & result;
    return result;
}

void SoapyRemoteDevice::writeGPIO(const std::string &bank, const unsigned value)
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_WRITE_GPIO;
    packer & bank;
    packer & int(value);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
}

void SoapyRemoteDevice::writeGPIO(const std::string &bank, const unsigned value, const unsigned mask)
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_WRITE_GPIO_MASKED;
    packer & bank;
    packer & int(value);
    packer & int(mask);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
}

unsigned SoapyRemoteDevice::readGPIO(const std::string &bank) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_READ_GPIO;
    packer & bank;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    int result;
    unpacker & result;
    return unsigned(result);
}

void SoapyRemoteDevice::writeGPIODir(const std::string &bank, const unsigned dir)
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_WRITE_GPIO_DIR;
    packer & bank;
    packer & int(dir);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
}

void SoapyRemoteDevice::writeGPIODir(const std::string &bank, const unsigned dir, const unsigned mask)
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_WRITE_GPIO_DIR_MASKED;
    packer & bank;
    packer & int(dir);
    packer & int(mask);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
}

unsigned SoapyRemoteDevice::readGPIODir(const std::string &bank) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_READ_GPIO_DIR;
    packer & bank;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    int result;
    unpacker & result;
    return unsigned(result);
}

/*******************************************************************
 * I2C API
 ******************************************************************/

void SoapyRemoteDevice::writeI2C(const int addr, const std::string &data)
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_WRITE_I2C;
    packer & int(addr);
    packer & data;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
}

std::string SoapyRemoteDevice::readI2C(const int addr, const size_t numBytes)
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_READ_I2C;
    packer & int(addr);
    packer & int(numBytes);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    std::string result;
    unpacker & result;
    return result;
}

/*******************************************************************
 * SPI API
 ******************************************************************/

unsigned SoapyRemoteDevice::transactSPI(const int addr, const unsigned data, const size_t numBits)
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_TRANSACT_SPI;
    packer & int(addr);
    packer & int(data);
    packer & int(numBits);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    int result;
    unpacker & result;
    return unsigned(result);
}

/*******************************************************************
 * UART API
 ******************************************************************/

std::vector<std::string> SoapyRemoteDevice::listUARTs(void) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_LIST_UARTS;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    std::vector<std::string> result;
    unpacker & result;
    return result;
}

void SoapyRemoteDevice::writeUART(const std::string &which, const std::string &data)
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_WRITE_UART;
    packer & which;
    packer & data;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
}

std::string SoapyRemoteDevice::readUART(const std::string &which, const long timeoutUs) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_READ_UART;
    packer & which;
    packer & int(timeoutUs);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    std::string result;
    unpacker & result;
    return result;
}
