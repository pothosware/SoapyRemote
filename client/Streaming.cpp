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
    const std::vector<size_t> &channels_,
    const SoapySDR::Kwargs &args)
{
    //default to channel 0 when not specified
    //the channels vector cannot be empty
    //its used for stream endpoint allocation
    auto channels = channels_;
    if (channels.empty()) channels.push_back(0);

    //extract remote endpoint format using special remoteFormat keyword
    //use the client's local format when the remote format is not specified
    auto remoteFormat = localFormat;
    const auto remoteFormatIt = args.find(SOAPY_REMOTE_KWARG_FORMAT);
    if (remoteFormatIt != args.end()) remoteFormat = remoteFormatIt->second;

    double scaleFactor = double(1 << ((formatToSize(remoteFormat)*4)-1));
    const auto scaleFactorIt = args.find(SOAPY_REMOTE_KWARG_SCALAR);
    if (scaleFactorIt != args.end()) scaleFactor = std::stod(scaleFactorIt->second);

    size_t mtu = SOAPY_REMOTE_DEFAULT_ENDPOINT_MTU;
    const auto mtuIt = args.find(SOAPY_REMOTE_KWARG_MTU);
    if (mtuIt != args.end()) mtu = size_t(std::stod(mtuIt->second));

    size_t window = SOAPY_REMOTE_DEFAULT_ENDPOINT_WINDOW;
    const auto windowIt = args.find(SOAPY_REMOTE_KWARG_WINDOW);
    if (windowIt != args.end()) window = size_t(std::stod(windowIt->second));

    SoapySDR::logf(SOAPY_SDR_INFO, "SoapyRemote::setup%sStream(remoteFormat=%s, localFormat=%s, scaleFactor=%g, mtu=%d, window=%d)",
        (direction == SOAPY_SDR_RX)?"Rx":"Tx", remoteFormat.c_str(), localFormat.c_str(), scaleFactor, int(mtu), int(window));

    //check supported formats
    ConvertTypes convertType = CONVERT_MEMCPY;
    if (localFormat == remoteFormat) convertType = CONVERT_MEMCPY;
    else if (localFormat == "CF32" and remoteFormat == "CS16") convertType = CONVERT_CF32_CS16;
    else if (localFormat == "CF32" and remoteFormat == "CS8") convertType = CONVERT_CF32_CS8;
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

    //extract socket node information
    const auto localNode = SoapyURL(_sock.getsockname()).getNode();
    const auto remoteNode = SoapyURL(_sock.getpeername()).getNode();

    //bind the stream socket to an automatic port
    const auto bindURL = SoapyURL("udp", localNode, "0").toString();
    int ret = data->streamSock.bind(bindURL);
    if (ret != 0)
    {
        const std::string errorMsg = data->streamSock.lastErrorMsg();
        delete data;
        throw std::runtime_error("SoapyRemote::setupStream("+bindURL+") -- bind FAIL: " + errorMsg);
    }
    SoapySDR::logf(SOAPY_SDR_INFO, "Client side stream bound to %s", data->streamSock.getsockname().c_str());
    const auto clientBindPort = SoapyURL(data->streamSock.getsockname()).getService();

    //bind the status socket to an automatic port
    ret = data->statusSock.bind(bindURL);
    if (ret != 0)
    {
        const std::string errorMsg = data->statusSock.lastErrorMsg();
        delete data;
        throw std::runtime_error("SoapyRemote::setupStream("+bindURL+") -- bind FAIL: " + errorMsg);
    }
    SoapySDR::logf(SOAPY_SDR_INFO, "Client side status bound to %s", data->streamSock.getsockname().c_str());
    const auto statusBindPort = SoapyURL(data->statusSock.getsockname()).getService();

    //setup the remote end of the stream
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_SETUP_STREAM;
    packer & char(direction);
    packer & remoteFormat;
    packer & channels;
    packer & args;
    packer & clientBindPort;
    packer & statusBindPort;
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    std::string serverBindPort;
    unpacker & data->streamId;
    unpacker & serverBindPort;

    //connect the stream socket to the specified port
    const auto connectURL = SoapyURL("udp", remoteNode, serverBindPort).toString();
    ret = data->streamSock.connect(connectURL);
    if (ret != 0)
    {
        const std::string errorMsg = data->streamSock.lastErrorMsg();
        delete data;
        throw std::runtime_error("SoapyRemote::setupStream("+connectURL+") -- connect FAIL: " + errorMsg);
    }
    SoapySDR::logf(SOAPY_SDR_INFO, "Client side stream connected to %s", data->streamSock.getpeername().c_str());

    //create endpoint
    data->endpoint = new SoapyStreamEndpoint(data->streamSock, data->statusSock,
        direction == SOAPY_SDR_RX, channels.size(), formatToSize(remoteFormat), mtu, window);

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

    //completed the buffer, release its handle
    if (data->readElemsLeft == 0)
    {
        this->releaseReadBuffer(stream, data->readHandle);
    }

    //increment pointers for the remainder conversion
    else
    {
        flags |= SOAPY_SDR_MORE_FRAGMENTS;
        const size_t offsetBytes = data->endpoint->getElemSize()*numSamples;
        for (size_t i = 0; i < data->recvBuffs.size(); i++)
        {
            data->recvBuffs[i] = ((char *)data->recvBuffs[i]) + offsetBytes;
        }
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
    size_t handle = 0;
    int ret = this->acquireWriteBuffer(stream, handle, data->sendBuffs.data(), timeoutUs);
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
    auto ep = data->endpoint;
    if (not ep->waitStatus(timeoutUs)) return SOAPY_SDR_TIMEOUT;
    return ep->readStatus(chanMask, flags, timeNs);
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
