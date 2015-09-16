// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyClient.hpp"
#include "ClientStreamData.hpp"
#include "SoapyRemoteDefs.hpp"
#include "FormatToElemSize.hpp"
#include "SoapyURLUtils.hpp"
#include "SoapyRPCPacker.hpp"
#include "SoapyRPCUnpacker.hpp"
#include "SoapyRecvEndpoint.hpp"
#include "SoapySendEndpoint.hpp"
#include <algorithm> //std::min

SoapySDR::Stream *SoapyRemoteDevice::setupStream(
    const int direction,
    const std::string &localFormat,
    const std::vector<size_t> &channels,
    const SoapySDR::Kwargs &args)
{
    //extract remote endpoint format using special remoteFormat keyword
    //use the client's local format when the remote format is not specified
    auto remoteFormat = localFormat;
    if (args.count("remoteFormat") != 0) remoteFormat = args.at("remoteFormat");

    auto scaleFactor = SOAPY_REMOTE_DEFAULT_SCALING;
    if (args.count("remoteScalar") != 0) scaleFactor = std::stod(args.at("remoteScalar"));

    size_t mtu = SOAPY_REMOTE_DEFAULT_ENDPOINT_MTU;
    if (args.count("remoteMTU") != 0) mtu = std::stoul(args.at("remoteMTU"));

    size_t window = SOAPY_REMOTE_DEFAULT_ENDPOINT_WINDOW;
    if (args.count("remoteWindow") != 0) window = std::stoul(args.at("remoteWindow"));

    //check supported formats
    ConvertTypes convertType = CONVERT_MEMCPY;
    if (localFormat == remoteFormat) convertType = CONVERT_MEMCPY;
    else if (localFormat == "CF32" and remoteFormat == "CS16") convertType = CONVERT_CF32_CS16;
    else throw std::runtime_error(
        "SoapyRemote::setupStream() conversion not supported;"
        "localFormat="+localFormat+", remoteFormat="+remoteFormat);

    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_SETUP_STREAM;
    packer & char(direction);
    packer & remoteFormat;
    packer & channels;
    packer & args;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    int streamId = 0;
    std::string remotePort;
    unpacker & streamId;
    unpacker & remotePort;

    //allocate new local stream data
    ClientStreamData *data = new ClientStreamData();
    data->localFormat = localFormat;
    data->remoteFormat = remoteFormat;
    data->streamId = streamId;
    data->recvBuffs.resize(channels.size());
    data->sendBuffs.resize(channels.size());
    data->convertType = convertType;
    data->scaleFactor = scaleFactor;

    //connect the UDP socket
    std::string scheme, node, service;
    splitURL(_sock.getpeername(), scheme, node, service);
    const auto streamURL = combineURL("udp", node, remotePort);
    int ret = data->sock.connect(streamURL);
    if (ret != 0)
    {
        const auto errorMsg = data->sock.lastErrorMsg();
        delete data;
        throw std::runtime_error("SoapyRemote::setupStream("+streamURL+") -- connect FAIL: " + errorMsg);
    }

    //create endpoint
    if (direction == SOAPY_SDR_TX) data->sendEndpoint = new SoapySendEndpoint(
        data->sock, channels.size(), formatToSize(localFormat), mtu, window);
    if (direction == SOAPY_SDR_RX) data->recvEndpoint = new SoapyRecvEndpoint(
        data->sock, channels.size(), formatToSize(localFormat), mtu, window);

    return (SoapySDR::Stream *)data;
}

void SoapyRemoteDevice::closeStream(SoapySDR::Stream *stream)
{
    auto data = (ClientStreamData *)stream;

    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_CLOSE_STREAM;
    packer & data->streamId;
    packer();

    SoapyRPCUnpacker unpacker(_sock);

    //cleanup local stream data
    delete data->recvEndpoint;
    delete data->sendEndpoint;
    delete data;
}

size_t SoapyRemoteDevice::getStreamMTU(SoapySDR::Stream *stream) const
{
    auto data = (ClientStreamData *)stream;
    if (data->recvEndpoint != nullptr) return data->recvEndpoint->getBuffSize();
    if (data->sendEndpoint != nullptr) return data->sendEndpoint->getBuffSize();
    return SoapySDR::Device::getStreamMTU(stream);
}

int SoapyRemoteDevice::activateStream(
    SoapySDR::Stream *stream,
    const int flags,
    const long long timeNs,
    const size_t numElems)
{
    auto data = (ClientStreamData *)stream;

    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_ACTIVATE_STREAM;
    packer & data->streamId;
    packer & flags;
    packer & timeNs;
    packer & int(numElems);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    int result = 0;
    unpacker & result;
    return result;
}

int SoapyRemoteDevice::deactivateStream(
    SoapySDR::Stream *stream,
    const int flags,
    const long long timeNs)
{
    auto data = (ClientStreamData *)stream;

    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_DEACTIVATE_STREAM;
    packer & data->streamId;
    packer & flags;
    packer & timeNs;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    int result = 0;
    unpacker & result;
    return result;
}

int SoapyRemoteDevice::readStream(
    SoapySDR::Stream *stream,
    void * const *buffs,
    const size_t numElems,
    int &flags,
    long long &timeNs,
    const long timeoutUs)
{
    auto data = (ClientStreamData *)stream;

    //call into direct buffer access (when there is no remainder)
    if (data->readElemsLeft == 0)
    {
        int ret = this->acquireReadBuffer(stream, data->readHandle, data->recvBuffs.data(), flags, timeNs, timeoutUs);
        if (ret < 0) return ret;
        data->readElemsLeft = size_t(ret);
    }

    //convert the buffer
    size_t numSamples = std::min(numElems, data->readElemsLeft);
    data->convertRecvBuffs(buffs, numSamples);
    data->readElemsLeft -= numSamples;
    if (data->readElemsLeft == 0)
    {
        this->releaseReadBuffer(stream, data->readHandle);
    }
    return numSamples;
}

int SoapyRemoteDevice::writeStream(
    SoapySDR::Stream *stream,
    const void * const *buffs,
    const size_t numElems,
    int &flags,
    const long long timeNs,
    const long timeoutUs)
{
    auto data = (ClientStreamData *)stream;

    //acquire from direct buffer access
    int handle = 0;
    int ret = this->acquireWriteBuffer(stream, data->readHandle, data->sendBuffs.data(), timeoutUs);
    if (ret < 0) return ret;

    //only end burst if the last sample can be released
    const size_t numSamples = std::min<size_t>(ret, numElems);
    if (numSamples < numElems) flags &= ~(SOAPY_SDR_END_BURST);

    //convert the samples
    data->convertSendBuffs(buffs, numSamples);

    //release to direct buffer access
    this->releaseWriteBuffer(stream, handle, numSamples, flags, timeNs);
    return numSamples;
}

int SoapyRemoteDevice::readStreamStatus(
    SoapySDR::Stream *stream,
    size_t &chanMask,
    int &flags,
    long long &timeNs,
    const long timeoutUs)
{
    auto data = (ClientStreamData *)stream;

    //TODO implement extra channel
}

/*******************************************************************
 * Direct buffer access API
 ******************************************************************/

size_t SoapyRemoteDevice::getNumDirectAccessBuffers(SoapySDR::Stream *stream)
{
    auto data = (ClientStreamData *)stream;
    if (data->recvEndpoint != nullptr) return data->recvEndpoint->getNumBuffs();
    if (data->sendEndpoint != nullptr) return data->sendEndpoint->getNumBuffs();
    return SoapySDR::Device::getNumDirectAccessBuffers(stream);
}

int SoapyRemoteDevice::getDirectAccessBufferAddrs(SoapySDR::Stream *stream, const size_t handle, void **buffs)
{
    auto data = (ClientStreamData *)stream;
    if (data->recvEndpoint != nullptr)
    {
        data->recvEndpoint->getAddrs(handle, buffs);
        return 0;
    }
    if (data->sendEndpoint != nullptr)
    {
        data->sendEndpoint->getAddrs(handle, buffs);
        return 0;
    }
    return SoapySDR::Device::getDirectAccessBufferAddrs(stream, handle, buffs);
}

int SoapyRemoteDevice::acquireReadBuffer(
    SoapySDR::Stream *stream,
    size_t &handle,
    const void **buffs,
    int &flags,
    long long &timeNs,
    const long timeoutUs)
{
    auto data = (ClientStreamData *)stream;
    auto recv = data->recvEndpoint;
    if (not recv->wait(timeoutUs)) return SOAPY_SDR_TIMEOUT;
    return recv->acquire(handle, buffs, flags, timeNs);
}

void SoapyRemoteDevice::releaseReadBuffer(
    SoapySDR::Stream *stream,
    const size_t handle)
{
    auto data = (ClientStreamData *)stream;
    auto recv = data->recvEndpoint;
    return recv->release(handle);
}

int SoapyRemoteDevice::acquireWriteBuffer(
    SoapySDR::Stream *stream,
    size_t &handle,
    void **buffs,
    const long timeoutUs)
{
    auto data = (ClientStreamData *)stream;
    auto send = data->sendEndpoint;
    if (not send->wait(timeoutUs)) return SOAPY_SDR_TIMEOUT;
    return send->acquire(handle, buffs);
}

void SoapyRemoteDevice::releaseWriteBuffer(
    SoapySDR::Stream *stream,
    const size_t handle,
    const size_t numElems,
    int &flags,
    const long long timeNs)
{
    auto data = (ClientStreamData *)stream;
    auto send = data->sendEndpoint;
    return send->release(handle, numElems, flags, timeNs);
}
