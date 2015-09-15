// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyClient.hpp"
#include "SoapyRPCPacker.hpp"
#include "SoapyRPCUnpacker.hpp"
#include "SoapyRecvEndpoint.hpp"
#include "SoapySendEndpoint.hpp"

struct SoapyRemoteStreamData
{
    SoapyRemoteStreamData(void):
        streamId(-1),
        recvEndpoint(nullptr),
        sendEndpoint(nullptr)
    {
        return;
    }

    //string formats in use
    std::string localFormat;
    std::string remoteFormat;

    //this ID identifies the stream to the remote host
    int streamId;

    //datagram socket for stream endpoint
    SoapyRPCSocket sock;

    //using one of the following endpoints
    SoapyRecvEndpoint *recvEndpoint;
    SoapySendEndpoint *sendEndpoint;
};

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
    unpacker & streamId;

    //allocate new local stream data
    SoapyRemoteStreamData *data = new SoapyRemoteStreamData();
    data->localFormat = localFormat;
    data->remoteFormat = remoteFormat;
    data->streamId = streamId;
    //TODO socket stuff
    return (SoapySDR::Stream *)data;
}

void SoapyRemoteDevice::closeStream(SoapySDR::Stream *stream)
{
    auto data = (SoapyRemoteStreamData *)stream;

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
    auto data = (SoapyRemoteStreamData *)stream;
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
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_ACTIVATE_STREAM;
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
    std::lock_guard<std::mutex> lock(_mutex);
    SoapyRPCPacker packer(_sock);
    packer & SOAPY_REMOTE_DEACTIVATE_STREAM;
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
    //TODO call into direct access API
    //TODO convert buffer to local format
    //TODO deal with remainder
}

int SoapyRemoteDevice::writeStream(
    SoapySDR::Stream *stream,
    const void * const *buffs,
    const size_t numElems,
    int &flags,
    const long long timeNs,
    const long timeoutUs)
{
    //TODO convert buffer to remote format
    //TODO call into direct access API
}

int SoapyRemoteDevice::readStreamStatus(
    SoapySDR::Stream *stream,
    size_t &chanMask,
    int &flags,
    long long &timeNs,
    const long timeoutUs)
{
    //TODO implement extra channel
}

/*******************************************************************
 * Direct buffer access API
 ******************************************************************/

size_t SoapyRemoteDevice::getNumDirectAccessBuffers(SoapySDR::Stream *stream)
{
    auto data = (SoapyRemoteStreamData *)stream;
    if (data->recvEndpoint != nullptr) return data->recvEndpoint->getNumBuffs();
    if (data->sendEndpoint != nullptr) return data->sendEndpoint->getNumBuffs();
    return SoapySDR::Device::getNumDirectAccessBuffers(stream);
}

int SoapyRemoteDevice::getDirectAccessBufferAddrs(SoapySDR::Stream *stream, const size_t handle, void **buffs)
{
    auto data = (SoapyRemoteStreamData *)stream;
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
    auto data = (SoapyRemoteStreamData *)stream;
    auto recv = data->recvEndpoint;
    if (not recv->wait(timeoutUs)) return SOAPY_SDR_TIMEOUT;
    return recv->acquire(handle, buffs, flags, timeNs);
}

void SoapyRemoteDevice::releaseReadBuffer(
    SoapySDR::Stream *stream,
    const size_t handle)
{
    auto data = (SoapyRemoteStreamData *)stream;
    auto recv = data->recvEndpoint;
    return recv->release(handle);
}

int SoapyRemoteDevice::acquireWriteBuffer(
    SoapySDR::Stream *stream,
    size_t &handle,
    void **buffs,
    const long timeoutUs)
{
    auto data = (SoapyRemoteStreamData *)stream;
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
    auto data = (SoapyRemoteStreamData *)stream;
    auto send = data->sendEndpoint;
    return send->release(handle, numElems, flags, timeNs);
}
