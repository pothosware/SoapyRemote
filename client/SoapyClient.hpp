// Copyright (c) 2015-2020 Josh Blum
// Copyright (c) 2016-2016 Bastille Networks
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include "SoapyRPCSocket.hpp"
#include <SoapySDR/Device.hpp>
#include <mutex>

class SoapyLogAcceptor;

class SoapyRemoteDevice : public SoapySDR::Device
{
public:
    SoapyRemoteDevice(const std::string &url, const SoapySDR::Kwargs &args);

    ~SoapyRemoteDevice(void);

    //! Helper routine to cache server discovery
    static std::vector<std::string> getServerURLs(const int ipVer, const long timeoutUs);

    /*******************************************************************
     * Identification API
     ******************************************************************/

    std::string getDriverKey(void) const;

    std::string getHardwareKey(void) const;

    SoapySDR::Kwargs getHardwareInfo(void) const;

    /*******************************************************************
     * Channels API
     ******************************************************************/

    void setFrontendMapping(const int direction, const std::string &mapping);

    std::string getFrontendMapping(const int direction) const;

    size_t getNumChannels(const int direction) const;

    SoapySDR::Kwargs getChannelInfo(const int direction, const size_t channel) const;

    bool getFullDuplex(const int direction, const size_t channel) const;

    /*******************************************************************
     * Stream API
     ******************************************************************/

    std::vector<std::string> __getRemoteOnlyStreamFormats(const int direction, const size_t channel) const;

    std::vector<std::string> getStreamFormats(const int direction, const size_t channel) const;

    std::string getNativeStreamFormat(const int direction, const size_t channel, double &fullScale) const;

    SoapySDR::ArgInfoList getStreamArgsInfo(const int direction, const size_t channel) const;

    SoapySDR::Stream *setupStream(
        const int direction,
        const std::string &format,
        const std::vector<size_t> &channels,
        const SoapySDR::Kwargs &args);

    void closeStream(SoapySDR::Stream *stream);

    size_t getStreamMTU(SoapySDR::Stream *stream) const;

    int activateStream(
        SoapySDR::Stream *stream,
        const int flags,
        const long long timeNs,
        const size_t numElems);

    int deactivateStream(
        SoapySDR::Stream *stream,
        const int flags,
        const long long timeNs);

    int readStream(
        SoapySDR::Stream *stream,
        void * const *buffs,
        const size_t numElems,
        int &flags,
        long long &timeNs,
        const long timeoutUs);

    int writeStream(
        SoapySDR::Stream *stream,
        const void * const *buffs,
        const size_t numElems,
        int &flags,
        const long long timeNs,
        const long timeoutUs);

    int readStreamStatus(
        SoapySDR::Stream *stream,
        size_t &chanMask,
        int &flags,
        long long &timeNs,
        const long timeoutUs);

    /*******************************************************************
     * Direct buffer access API
     ******************************************************************/

    size_t getNumDirectAccessBuffers(SoapySDR::Stream *stream);

    int getDirectAccessBufferAddrs(SoapySDR::Stream *stream, const size_t handle, void **buffs);

    int acquireReadBuffer(
        SoapySDR::Stream *stream,
        size_t &handle,
        const void **buffs,
        int &flags,
        long long &timeNs,
        const long timeoutUs);

    void releaseReadBuffer(
        SoapySDR::Stream *stream,
        const size_t handle);

    int acquireWriteBuffer(
        SoapySDR::Stream *stream,
        size_t &handle,
        void **buffs,
        const long timeoutUs);

    void releaseWriteBuffer(
        SoapySDR::Stream *stream,
        const size_t handle,
        const size_t numElems,
        int &flags,
        const long long timeNs);

    /*******************************************************************
     * Antenna API
     ******************************************************************/

    std::vector<std::string> listAntennas(const int direction, const size_t channel) const;

    void setAntenna(const int direction, const size_t channel, const std::string &name);

    std::string getAntenna(const int direction, const size_t channel) const;

    /*******************************************************************
     * Frontend corrections API
     ******************************************************************/

    bool hasDCOffsetMode(const int direction, const size_t channel) const;

    void setDCOffsetMode(const int direction, const size_t channel, const bool automatic);

    bool getDCOffsetMode(const int direction, const size_t channel) const;

    bool hasDCOffset(const int direction, const size_t channel) const;

    void setDCOffset(const int direction, const size_t channel, const std::complex<double> &offset);

    std::complex<double> getDCOffset(const int direction, const size_t channel) const;

    bool hasIQBalance(const int direction, const size_t channel) const;

    void setIQBalance(const int direction, const size_t channel, const std::complex<double> &balance);

    std::complex<double> getIQBalance(const int direction, const size_t channel) const;

    bool hasIQBalanceMode(const int direction, const size_t channel) const;

    void setIQBalanceMode(const int direction, const size_t channel, const bool automatic);

    bool getIQBalanceMode(const int direction, const size_t channel) const;

    bool hasFrequencyCorrection(const int direction, const size_t channel) const;

    void setFrequencyCorrection(const int direction, const size_t channel, const double value);

    double getFrequencyCorrection(const int direction, const size_t channel) const;

    /*******************************************************************
     * Gain API
     ******************************************************************/

    std::vector<std::string> listGains(const int direction, const size_t channel) const;

    bool hasGainMode(const int direction, const size_t channel) const;

    void setGainMode(const int direction, const size_t channel, const bool automatic);

    bool getGainMode(const int direction, const size_t channel) const;

    void setGain(const int direction, const size_t channel, const double value);

    void setGain(const int direction, const size_t channel, const std::string &name, const double value);

    double getGain(const int direction, const size_t channel) const;

    double getGain(const int direction, const size_t channel, const std::string &name) const;

    SoapySDR::Range getGainRange(const int direction, const size_t channel) const;

    SoapySDR::Range getGainRange(const int direction, const size_t channel, const std::string &name) const;

    /*******************************************************************
     * Frequency API
     ******************************************************************/

    void setFrequency(const int direction, const size_t channel, const double frequency, const SoapySDR::Kwargs &args);

    void setFrequency(const int direction, const size_t channel, const std::string &name, const double frequency, const SoapySDR::Kwargs &args);

    double getFrequency(const int direction, const size_t channel) const;

    double getFrequency(const int direction, const size_t channel, const std::string &name) const;

    std::vector<std::string> listFrequencies(const int direction, const size_t channel) const;

    SoapySDR::RangeList getFrequencyRange(const int direction, const size_t channel) const;

    SoapySDR::RangeList getFrequencyRange(const int direction, const size_t channel, const std::string &name) const;

    SoapySDR::ArgInfoList getFrequencyArgsInfo(const int direction, const size_t channel) const;

    /*******************************************************************
     * Sample Rate API
     ******************************************************************/

    void setSampleRate(const int direction, const size_t channel, const double rate);

    double getSampleRate(const int direction, const size_t channel) const;

    std::vector<double> listSampleRates(const int direction, const size_t channel) const;

    SoapySDR::RangeList getSampleRateRange(const int direction, const size_t channel) const;

    /*******************************************************************
     * Bandwidth API
     ******************************************************************/

    void setBandwidth(const int direction, const size_t channel, const double bw);

    double getBandwidth(const int direction, const size_t channel) const;

    std::vector<double> listBandwidths(const int direction, const size_t channel) const;

    SoapySDR::RangeList getBandwidthRange(const int direction, const size_t channel) const;

    /*******************************************************************
     * Clocking API
     ******************************************************************/

    void setMasterClockRate(const double rate);

    double getMasterClockRate(void) const;

    SoapySDR::RangeList getMasterClockRates(void) const;

    void setReferenceClockRate(const double rate);

    double getReferenceClockRate(void) const;

    SoapySDR::RangeList getReferenceClockRates(void) const;

    std::vector<std::string> listClockSources(void) const;

    void setClockSource(const std::string &source);

    std::string getClockSource(void) const;

    /*******************************************************************
     * Time API
     ******************************************************************/

    std::vector<std::string> listTimeSources(void) const;

    void setTimeSource(const std::string &source);

    std::string getTimeSource(void) const;

    bool hasHardwareTime(const std::string &what) const;

    long long getHardwareTime(const std::string &what) const;

    void setHardwareTime(const long long timeNs, const std::string &what);

    void setCommandTime(const long long timeNs, const std::string &what);

    /*******************************************************************
     * Sensor API
     ******************************************************************/

    std::vector<std::string> listSensors(void) const;

    SoapySDR::ArgInfo getSensorInfo(const std::string &name) const;

    std::string readSensor(const std::string &name) const;

    std::vector<std::string> listSensors(const int direction, const size_t channel) const;

    SoapySDR::ArgInfo getSensorInfo(const int direction, const size_t channel, const std::string &name) const;

    std::string readSensor(const int direction, const size_t channel, const std::string &name) const;

    /*******************************************************************
     * Register API
     ******************************************************************/

    std::vector<std::string> listRegisterInterfaces(void) const;

    void writeRegister(const std::string &name, const unsigned addr, const unsigned value);

    unsigned readRegister(const std::string &name, const unsigned addr) const;

    void writeRegister(const unsigned addr, const unsigned value);

    unsigned readRegister(const unsigned addr) const;

    void writeRegisters(const std::string &name, const unsigned addr, const std::vector<unsigned> &value);

    std::vector<unsigned> readRegisters(const std::string &name, const unsigned addr, const size_t length) const;

    /*******************************************************************
     * Settings API
     ******************************************************************/

    SoapySDR::ArgInfoList getSettingInfo(void) const;

    void writeSetting(const std::string &key, const std::string &value);

    std::string readSetting(const std::string &key) const;

    SoapySDR::ArgInfoList getSettingInfo(const int direction, const size_t channel) const;

    void writeSetting(const int direction, const size_t channel, const std::string &key, const std::string &value);

    std::string readSetting(const int direction, const size_t channel, const std::string &key) const;

    /*******************************************************************
     * GPIO API
     ******************************************************************/

    std::vector<std::string> listGPIOBanks(void) const;

    void writeGPIO(const std::string &bank, const unsigned value);

    void writeGPIO(const std::string &bank, const unsigned value, const unsigned mask);

    unsigned readGPIO(const std::string &bank) const;

    void writeGPIODir(const std::string &bank, const unsigned dir);

    void writeGPIODir(const std::string &bank, const unsigned dir, const unsigned mask);

    unsigned readGPIODir(const std::string &bank) const;

    /*******************************************************************
     * I2C API
     ******************************************************************/

    void writeI2C(const int addr, const std::string &data);

    std::string readI2C(const int addr, const size_t numBytes);

    /*******************************************************************
     * SPI API
     ******************************************************************/

    unsigned transactSPI(const int addr, const unsigned data, const size_t numBits);

    /*******************************************************************
     * UART API
     ******************************************************************/

    std::vector<std::string> listUARTs(void) const;

    void writeUART(const std::string &which, const std::string &data);

    std::string readUART(const std::string &which, const long timeoutUs) const;

private:
    SoapySocketSession _sess;
    mutable SoapyRPCSocket _sock;
    SoapyLogAcceptor *_logAcceptor;
    mutable std::mutex _mutex;
    std::string _defaultStreamProt;
};
