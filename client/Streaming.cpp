// Copyright (c) 2015-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <SoapySDR/Logger.hpp>
#include <SoapySDR/Formats.hpp>
#include "SoapyClient.hpp"
#include "ClientStreamData.hpp"
#include "SoapyRemoteDefs.hpp"
#include "SoapyURLUtils.hpp"
#include "SoapyRPCPacker.hpp"
#include "SoapyRPCUnpacker.hpp"
#include "SoapyStreamEndpoint.hpp"
#include <algorithm> //std::min, std::find
#include <memory> //unique_ptr

std::vector<std::string> SoapyRemoteDevice::__getRemoteOnlyStreamFormats(const int direction, const size_t channel) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_STREAM_FORMATS;
    packer & char(direction);
    packer & int(channel);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    std::vector<std::string> result;
    unpacker & result;
    return result;
}

std::vector<std::string> SoapyRemoteDevice::getStreamFormats(const int direction, const size_t channel) const
{
    auto formats = __getRemoteOnlyStreamFormats(direction, channel);

    //add complex floats when a conversion is possible
    const bool hasCF32 = std::find(formats.begin(), formats.end(), SOAPY_SDR_CF32) != formats.end();
    const bool hasCS16 = std::find(formats.begin(), formats.end(), SOAPY_SDR_CS16) != formats.end();
    const bool hasCS8 = std::find(formats.begin(), formats.end(), SOAPY_SDR_CS8) != formats.end();
    const bool hasCU8 = std::find(formats.begin(), formats.end(), SOAPY_SDR_CU8) != formats.end();
    if (not hasCF32 and (hasCS16 or hasCS8 or hasCU8)) formats.push_back(SOAPY_SDR_CF32);

    return formats;
}

std::string SoapyRemoteDevice::getNativeStreamFormat(const int direction, const size_t channel, double &fullScale) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_GET_NATIVE_STREAM_FORMAT;
    packer & char(direction);
    packer & int(channel);
    packer();

    SoapyRPCUnpacker unpacker(_sock);
    std::string result;
    unpacker & result;
    unpacker & fullScale;
    return result;
}

SoapySDR::ArgInfoList SoapyRemoteDevice::getStreamArgsInfo(const int direction, const size_t channel) const
{
    //get the remote arguments first (careful with lock scope)
    SoapySDR::ArgInfoList result;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        SoapyRPCPacker packer(_sock);
        packer & SOAPY_REMOTE_GET_STREAM_ARGS_INFO;
        packer & char(direction);
        packer & int(channel);
        packer();

        SoapyRPCUnpacker unpacker(_sock);
        unpacker & result;
    }

    //insert SoapyRemote stream arguments
    double fullScale = 0.0;
    SoapySDR::ArgInfo formatArg;
    formatArg.key = "remote:format";
    formatArg.value = this->getNativeStreamFormat(direction, channel, fullScale);
    formatArg.name = "Remote Format";
    formatArg.description = "The stream format used on the remote device.";
    formatArg.type = SoapySDR::ArgInfo::STRING;
    formatArg.options = __getRemoteOnlyStreamFormats(direction, channel);
    result.push_back(formatArg);

    SoapySDR::ArgInfo scaleArg;
    scaleArg.key = "remote:scale";
    scaleArg.value = std::to_string(fullScale);
    scaleArg.name = "Remote Scale";
    scaleArg.description = "The factor used to scale remote samples to full-scale floats.";
    scaleArg.type = SoapySDR::ArgInfo::FLOAT;
    result.push_back(scaleArg);

    SoapySDR::ArgInfo mtuArg;
    mtuArg.key = "remote:mtu";
    mtuArg.value = std::to_string(SOAPY_REMOTE_DEFAULT_ENDPOINT_MTU);
    mtuArg.name = "Remote MTU";
    mtuArg.units = "bytes";
    mtuArg.description = "The maximum datagram transfer size in bytes.";
    mtuArg.type = SoapySDR::ArgInfo::INT;
    result.push_back(mtuArg);

    SoapySDR::ArgInfo windowArg;
    windowArg.key = "remote:window";
    windowArg.value = std::to_string(SOAPY_REMOTE_DEFAULT_ENDPOINT_WINDOW);
    windowArg.name = "Remote Window";
    windowArg.units = "bytes";
    windowArg.description = "The size of the kernel socket buffer in bytes.";
    windowArg.type = SoapySDR::ArgInfo::INT;
    result.push_back(windowArg);

    SoapySDR::ArgInfo priorityArg;
    priorityArg.key = "remote:priority";
    priorityArg.value = std::to_string(SOAPY_REMOTE_DEFAULT_THREAD_PRIORITY);
    priorityArg.name = "Remote Priority";
    priorityArg.description = "Specify the scheduling priority of the server forwarding threads.";
    priorityArg.type = SoapySDR::ArgInfo::FLOAT;
    priorityArg.range = SoapySDR::Range(-1.0, 1.0);
    result.push_back(priorityArg);

    SoapySDR::ArgInfo protArg;
    protArg.key = "remote:prot";
    protArg.value = "udp";
    protArg.name = "Remote Protocol";
    protArg.description = "Specify the transport protocol for the remote stream.";
    protArg.type = SoapySDR::ArgInfo::STRING;
    protArg.options = {"udp", "tcp", "none"};
    result.push_back(protArg);

    return result;
}

SoapySDR::Stream *SoapyRemoteDevice::setupStream(
    const int direction,
    const std::string &localFormat,
    const std::vector<size_t> &channels_,
    const SoapySDR::Kwargs &args_)
{
    SoapySDR::Kwargs args(args_); //read/write copy

    //extract the specified stream protocol
    std::string prot = _defaultStreamProt;
    const auto protIt = args.find(SOAPY_REMOTE_KWARG_PROT);
    if (protIt != args.end()) prot = protIt->second;

    //setup the stream in bypass mode for protocol none
    if (prot == "none")
    {
        auto data = std::unique_ptr<ClientStreamData>(new ClientStreamData());
        std::lock_guard<std::mutex> lock(_mutex);
        SoapyRPCPacker packer(_sock);
        packer & SOAPY_REMOTE_SETUP_STREAM_BYPASS;
        packer & char(direction);
        packer & localFormat;
        packer & channels_;
        packer & args_;
        packer();
        SoapyRPCUnpacker unpacker(_sock);
        unpacker & data->streamId;
        return (SoapySDR::Stream *)data.release();
    }

    //default to channel 0 when not specified
    //the channels vector cannot be empty
    //its used for stream endpoint allocation
    auto channels = channels_;
    if (channels.empty()) channels.push_back(0);

    //use the remote device's native stream format and scale factor when the conversion is supported
    double nativeScaleFactor = 0.0;
    auto nativeFormat = this->getNativeStreamFormat(direction, channels.front(), nativeScaleFactor);
    const bool useNative = (localFormat == nativeFormat) or
        (localFormat == SOAPY_SDR_CF32 and nativeFormat == SOAPY_SDR_CS16) or
        (localFormat == SOAPY_SDR_CF32 and nativeFormat == SOAPY_SDR_CS12) or
        (localFormat == SOAPY_SDR_CS16 and nativeFormat == SOAPY_SDR_CS12) or
        (localFormat == SOAPY_SDR_CS16 and nativeFormat == SOAPY_SDR_CS8) or
        (localFormat == SOAPY_SDR_CF32 and nativeFormat == SOAPY_SDR_CS8) or
        (localFormat == SOAPY_SDR_CF32 and nativeFormat == SOAPY_SDR_CU8);

    //use the native format when the conversion is supported,
    //otherwise use the client's local format for the default
    auto remoteFormat = useNative?nativeFormat:localFormat;
    const auto remoteFormatIt = args.find(SOAPY_REMOTE_KWARG_FORMAT);
    if (remoteFormatIt != args.end()) remoteFormat = remoteFormatIt->second;

    //use the native scale factor when the remote format is native,
    //otherwise the default scale factor is the max signed integer
    double scaleFactor = (remoteFormat == nativeFormat)?nativeScaleFactor:double(1 << ((SoapySDR::formatToSize(remoteFormat)*4)-1));
    const auto scaleFactorIt = args.find(SOAPY_REMOTE_KWARG_SCALE);
    if (scaleFactorIt != args.end()) scaleFactor = std::stod(scaleFactorIt->second);

    //determine reliable stream mode with tcp or datagram mode
    const bool datagramMode = (prot == "udp");
    if (prot == "udp") {}
    else if (prot == "tcp") {}
    else throw std::runtime_error(
        "SoapyRemote::setupStream() protcol not supported;"
        "expected 'udp' or 'tcp', but got '"+prot+"'");
    args[SOAPY_REMOTE_KWARG_PROT] = prot;

    size_t mtu = datagramMode?SOAPY_REMOTE_DEFAULT_ENDPOINT_MTU:SOAPY_REMOTE_SOCKET_BUFFMAX;
    const auto mtuIt = args.find(SOAPY_REMOTE_KWARG_MTU);
    if (mtuIt != args.end()) mtu = size_t(std::stod(mtuIt->second));
    args[SOAPY_REMOTE_KWARG_MTU] = std::to_string(mtu);

    size_t window = SOAPY_REMOTE_DEFAULT_ENDPOINT_WINDOW;
    const auto windowIt = args.find(SOAPY_REMOTE_KWARG_WINDOW);
    if (windowIt != args.end()) window = size_t(std::stod(windowIt->second));
    args[SOAPY_REMOTE_KWARG_WINDOW] = std::to_string(window);

    SoapySDR::logf(SOAPY_SDR_INFO, "SoapyRemote::setup%sStream(remoteFormat=%s, localFormat=%s, scaleFactor=%g, mtu=%d, window=%d)",
        (direction == SOAPY_SDR_RX)?"Rx":"Tx", remoteFormat.c_str(), localFormat.c_str(), scaleFactor, int(mtu), int(window));

    //check supported formats
    ConvertTypes convertType = CONVERT_MEMCPY;
    if (localFormat == remoteFormat) convertType = CONVERT_MEMCPY;
    else if (localFormat == SOAPY_SDR_CF32 and remoteFormat == SOAPY_SDR_CS16) convertType = CONVERT_CF32_CS16;
    else if (localFormat == SOAPY_SDR_CF32 and remoteFormat == SOAPY_SDR_CS12) convertType = CONVERT_CF32_CS12;
    else if (localFormat == SOAPY_SDR_CS16 and remoteFormat == SOAPY_SDR_CS12) convertType = CONVERT_CS16_CS12;
    else if (localFormat == SOAPY_SDR_CS16 and remoteFormat == SOAPY_SDR_CS8) convertType = CONVERT_CS16_CS8;
    else if (localFormat == SOAPY_SDR_CF32 and remoteFormat == SOAPY_SDR_CS8) convertType = CONVERT_CF32_CS8;
    else if (localFormat == SOAPY_SDR_CF32 and remoteFormat == SOAPY_SDR_CU8) convertType = CONVERT_CF32_CU8;
    else throw std::runtime_error(
        "SoapyRemote::setupStream() conversion not supported;"
        "localFormat="+localFormat+", remoteFormat="+remoteFormat);

    //allocate new local stream data
    auto data = std::unique_ptr<ClientStreamData>(new ClientStreamData());
    data->localFormat = localFormat;
    data->remoteFormat = remoteFormat;
    data->recvBuffs.resize(channels.size());
    data->sendBuffs.resize(channels.size());
    data->convertType = convertType;
    data->scaleFactor = scaleFactor;

    //extract socket node information
    const auto localNode = SoapyURL(_sock.getsockname()).getNode();
    const auto remoteNode = SoapyURL(_sock.getpeername()).getNode();

    //bind the receiver side of the sockets in datagram mode
    std::string clientBindPort, statusBindPort;
    if (datagramMode)
    {
        //bind the stream socket to an automatic port
        const auto bindURL = SoapyURL("udp", localNode, "0").toString();
        int ret = data->streamSock.bind(bindURL);
        if (ret != 0)
        {
            const std::string errorMsg = data->streamSock.lastErrorMsg();
            throw std::runtime_error("SoapyRemote::setupStream("+bindURL+") -- bind FAIL: " + errorMsg);
        }
        SoapySDR::logf(SOAPY_SDR_INFO, "Client side stream bound to %s", data->streamSock.getsockname().c_str());
        clientBindPort = SoapyURL(data->streamSock.getsockname()).getService();

        //bind the status socket to an automatic port
        ret = data->statusSock.bind(bindURL);
        if (ret != 0)
        {
            const std::string errorMsg = data->statusSock.lastErrorMsg();
            throw std::runtime_error("SoapyRemote::setupStream("+bindURL+") -- bind FAIL: " + errorMsg);
        }
        SoapySDR::logf(SOAPY_SDR_INFO, "Client side status bound to %s", data->statusSock.getsockname().c_str());
        statusBindPort = SoapyURL(data->statusSock.getsockname()).getService();
    }

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

    //for tcp mode: get the binding port here and connect to it
    std::string serverBindPort;
    if (not datagramMode)
    {
        SoapyRPCUnpacker unpackerTcp(_sock);
        unpackerTcp & serverBindPort;
        const auto connectURL = SoapyURL(prot, remoteNode, serverBindPort).toString();
        int ret = data->streamSock.connect(connectURL);
        if (ret != 0)
        {
            const std::string errorMsg = data->streamSock.lastErrorMsg();
            throw std::runtime_error("SoapyRemote::setupStream("+connectURL+") -- connect FAIL: " + errorMsg);
        }
        ret = data->statusSock.connect(connectURL);
        if (ret != 0)
        {
            const std::string errorMsg = data->statusSock.lastErrorMsg();
            throw std::runtime_error("SoapyRemote::setupStream("+connectURL+") -- connect FAIL: " + errorMsg);
        }
    }

    //and wait for the response with binding port and stream id
    SoapyRPCUnpacker unpacker(_sock);
    unpacker & data->streamId;
    unpacker & serverBindPort;

    //connect the sending end of the stream socket
    if (datagramMode)
    {
        //connect the stream socket to the specified port
        const auto connectURL = SoapyURL(prot, remoteNode, serverBindPort).toString();
        int ret = data->streamSock.connect(connectURL);
        if (ret != 0)
        {
            const std::string errorMsg = data->streamSock.lastErrorMsg();
            throw std::runtime_error("SoapyRemote::setupStream("+connectURL+") -- connect FAIL: " + errorMsg);
        }
        SoapySDR::logf(SOAPY_SDR_INFO, "Client side stream connected to %s", data->streamSock.getpeername().c_str());
    }

    //create endpoint
    data->endpoint = new SoapyStreamEndpoint(data->streamSock, data->statusSock,
        datagramMode, direction == SOAPY_SDR_RX, channels.size(),
        SoapySDR::formatToSize(remoteFormat), mtu, window);

    return (SoapySDR::Stream *)data.release();
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
