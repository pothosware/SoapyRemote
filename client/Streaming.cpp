// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <SoapySDR/Logger.hpp>
#include "SoapyClient.hpp"
#include "ClientStreamData.hpp"
#include "SoapyRemoteDefs.hpp"
#include "FormatToElemSize.hpp"
#include "SoapyURLUtils.hpp"
#include "SoapyRPCPacker.hpp"
#include "SoapyRPCUnpacker.hpp"
#include "SoapyStreamEndpoint.hpp"
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

    //allocate new local stream data
    ClientStreamData *data = new ClientStreamData();
    data->localFormat = localFormat;
    data->remoteFormat = remoteFormat;
    data->recvBuffs.resize(channels.size());
    data->sendBuffs.resize(channels.size());
    data->convertType = convertType;
    data->scaleFactor = scaleFactor;

    //first bind a socket to automatic port
    //the server connect via allocated port
    std::string clientBindPort;
    std::string scheme, node, service;
    splitURL(_sock.getpeername(), scheme, node, service);
    const auto bindURL = combineURL("udp", node, "0");
    int ret = data->sock.bind(bindURL);
    if (ret != 0)
    {
        const std::string errorMsg = data->sock.lastErrorMsg();
        delete data;
        throw std::runtime_error("SoapyRemote::setupStream("+bindURL+") -- bind FAIL: " + errorMsg);
    }
    SoapySDR::logf(SOAPY_SDR_INFO, "Client side bound to %s", data->sock.getsockname().c_str());
    splitURL(data->sock.getsockname(), scheme, node, clientBindPort);

    //setup the remote end of the stream
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_SETUP_STREAM;
    packer & char(direction);
    packer & remoteFormat;
    packer & channels;
    packer & args;
    packer & clientBindPort;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    std::string serverBindPort;
    unpacker & data->streamId;
    unpacker & serverBindPort;

    //connect the UDP socket
    splitURL(_sock.getpeername(), scheme, node, service);
    const auto connectURL = combineURL("udp", node, serverBindPort);
    ret = data->sock.connect(connectURL);
    if (ret != 0)
    {
        const std::string errorMsg = data->sock.lastErrorMsg();
        delete data;
        throw std::runtime_error("SoapyRemote::setupStream("+connectURL+") -- connect FAIL: " + errorMsg);
    }
    SoapySDR::logf(SOAPY_SDR_INFO, "Client side connected to %s", data->sock.getpeername().c_str());

    //create endpoint
    data->endpoint = new SoapyStreamEndpoint(data->sock,
        direction == SOAPY_SDR_RX, channels.size(), formatToSize(localFormat), mtu, window);

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
    delete data->endpoint;
    delete data;
}

size_t SoapyRemoteDevice::getStreamMTU(SoapySDR::Stream *stream) const
{
    auto data = (ClientStreamData *)stream;
    return data->endpoint->getBuffSize();
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
    return data->endpoint->getNumBuffs();
}

int SoapyRemoteDevice::getDirectAccessBufferAddrs(SoapySDR::Stream *stream, const size_t handle, void **buffs)
{
    auto data = (ClientStreamData *)stream;
    data->endpoint->getAddrs(handle, buffs);
    return 0;
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
    auto ep = data->endpoint;
    if (not ep->waitRecv(timeoutUs)) return SOAPY_SDR_TIMEOUT;
    return ep->acquireRecv(handle, buffs, flags, timeNs);
}

void SoapyRemoteDevice::releaseReadBuffer(
    SoapySDR::Stream *stream,
    const size_t handle)
{
    auto data = (ClientStreamData *)stream;
    auto ep = data->endpoint;
    return ep->releaseRecv(handle);
}

int SoapyRemoteDevice::acquireWriteBuffer(
    SoapySDR::Stream *stream,
    size_t &handle,
    void **buffs,
    const long timeoutUs)
{
    auto data = (ClientStreamData *)stream;
    auto ep = data->endpoint;
    if (not ep->waitSend(timeoutUs)) return SOAPY_SDR_TIMEOUT;
    return ep->acquireSend(handle, buffs);
}

void SoapyRemoteDevice::releaseWriteBuffer(
    SoapySDR::Stream *stream,
    const size_t handle,
    const size_t numElems,
    int &flags,
    const long long timeNs)
{
    auto data = (ClientStreamData *)stream;
    auto ep = data->endpoint;
    return ep->releaseSend(handle, numElems, flags, timeNs);
}
