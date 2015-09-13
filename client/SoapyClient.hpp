// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyRemoteCommon.hpp"
#include "SoapyRPCSocket.hpp"
#include "SoapySocketSession.hpp"
#include <SoapySDR/Device.hpp>
#include <mutex>

class SoapyRemoteDevice : public SoapySDR::Device
{
public:
    SoapyRemoteDevice(const SoapySDR::Kwargs &args);

    ~SoapyRemoteDevice(void);

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

    bool getFullDuplex(const int direction, const size_t channel) const;

    /*******************************************************************
     * Stream API
     ******************************************************************/

    //TODO later, will be more complex than just the calls

    /*******************************************************************
     * Direct buffer access API
     ******************************************************************/

    //TODO later, will be more complex than just the calls


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

    /*******************************************************************
     * Gain API
     ******************************************************************/

    std::vector<std::string> listGains(const int direction, const size_t channel) const;

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

    /*******************************************************************
     * Sample Rate API
     ******************************************************************/

    void setSampleRate(const int direction, const size_t channel, const double rate);

    double getSampleRate(const int direction, const size_t channel) const;

    std::vector<double> listSampleRates(const int direction, const size_t channel) const;

    void setBandwidth(const int direction, const size_t channel, const double bw);

    double getBandwidth(const int direction, const size_t channel) const;

    std::vector<double> listBandwidths(const int direction, const size_t channel) const;

    /*******************************************************************
     * Clocking API
     ******************************************************************/

    void setMasterClockRate(const double rate);

    double getMasterClockRate(void) const;

    std::vector<std::string> listClockSources(void) const;

    void setClockSource(const std::string &source);

    std::string getClockSource(void) const;

    std::vector<std::string> listTimeSources(void) const;

    void setTimeSource(const std::string &source);

    std::string getTimeSource(void) const;

    /*******************************************************************
     * Time API
     ******************************************************************/

    bool hasHardwareTime(const std::string &what) const;

    long long getHardwareTime(const std::string &what) const;

    void setHardwareTime(const long long timeNs, const std::string &what);

    void setCommandTime(const long long timeNs, const std::string &what);

    /*******************************************************************
     * Sensor API
     ******************************************************************/

    std::vector<std::string> listSensors(void) const;

    std::string readSensor(const std::string &name) const;

    std::vector<std::string> listSensors(const int direction, const size_t channel) const;

    std::string readSensor(const int direction, const size_t channel, const std::string &name) const;

    /*******************************************************************
     * Register API
     ******************************************************************/

    void writeRegister(const unsigned addr, const unsigned value);

    unsigned readRegister(const unsigned addr) const;

    /*******************************************************************
     * Settings API
     ******************************************************************/

    void writeSetting(const std::string &key, const std::string &value);

    std::string readSetting(const std::string &key) const;

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
    SoapyRPCSocket _sock;
    std::mutex _mutex;
};
