// Copyright (c) 2018-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyMDNSEndpoint.hpp"
#include "SoapyRemoteDefs.hpp"
#include <SoapySDR/Logger.hpp>
#include "SoapyURLUtils.hpp"
#include <dns_sd.h>
#include <cerrno>
#include <cstdlib> //atoi

/***********************************************************************
 * Storage for mdns services
 **********************************************************************/
struct SoapyMDNSEndpointData
{
    SoapyMDNSEndpointData(void);
    ~SoapyMDNSEndpointData(void);
    DNSServiceRef sdRef;
};

SoapyMDNSEndpointData::SoapyMDNSEndpointData(void):
    sdRef(nullptr)
{
    return;
}

SoapyMDNSEndpointData::~SoapyMDNSEndpointData(void)
{
    if (sdRef != nullptr) DNSServiceRefDeallocate(sdRef);
}

/***********************************************************************
 * SoapyMDNSEndpoint interface hooks
 **********************************************************************/
SoapyMDNSEndpoint::SoapyMDNSEndpoint(void):
    _impl(new SoapyMDNSEndpointData())
{
    return;
}

SoapyMDNSEndpoint::~SoapyMDNSEndpoint(void)
{
    delete _impl;
}

void SoapyMDNSEndpoint::printInfo(void)
{
    static const int mDNSResponderVersion(_DNS_SD_H+0);
    SoapySDR::logf(SOAPY_SDR_INFO, "mDNSResponder version: v%d.%d.%d",
        mDNSResponderVersion/10000, //major
        (mDNSResponderVersion/100)%100, //minor
        mDNSResponderVersion%100); //build

    uint32_t version;
    uint32_t size = sizeof(version);
    auto ret = DNSServiceGetProperty(kDNSServiceProperty_DaemonVersion, &version, &size);
    if (ret == kDNSServiceErr_NoError) SoapySDR::logf(
        SOAPY_SDR_INFO, "Bonjour daemon version: v%d.%d.%d",
        version/10000, (version/100)%100, version%100);
}

bool SoapyMDNSEndpoint::status(void)
{
    return true;
}

static void registerServiceCallback(
    DNSServiceRef sdRef,
    DNSServiceFlags flags,
    DNSServiceErrorType errorCode,
    const char                          *name,
    const char                          *regtype,
    const char                          *domain,
    void                                *context)
{
    char fullname[kDNSServiceMaxDomainName];
    DNSServiceConstructFullName(fullname, name, regtype, domain);
    SoapySDR::logf(SOAPY_SDR_DEBUG, "SoapyMDNS resolving %s...", fullname);

    if (errorCode != kDNSServiceErr_NoError) return SoapySDR::logf(
        SOAPY_SDR_ERROR, "SoapyMDNS registerServiceCallback(%s) error: %d",
        fullname, errorCode);

    SoapySDR::logf(SOAPY_SDR_INFO, "DNSServiceRegister(%s)", fullname);
}

void SoapyMDNSEndpoint::registerService(const std::string &uuid, const std::string &service, const int ipVer)
{
    //text record with uuid
    TXTRecordRef txtRecord;
    TXTRecordCreate(&txtRecord, 0, nullptr);
    auto ret = TXTRecordSetValue(&txtRecord, "uuid", uuid.size(), uuid.data());
    if (ret != kDNSServiceErr_NoError) return SoapySDR::logf(
        SOAPY_SDR_ERROR, "TXTRecordSetValue() failed %d", ret);

    ret = DNSServiceRegister(
        &_impl->sdRef,
        DNSServiceFlags(0),
        kDNSServiceInterfaceIndexAny,
        nullptr, //use hostname
        SOAPY_REMOTE_DNSSD_TYPE,
        nullptr, //domain automatic
        nullptr, //host automatic
        htons(atoi(service.c_str())),
        TXTRecordGetLength(&txtRecord),
        TXTRecordGetBytesPtr(&txtRecord),
        &registerServiceCallback,
        _impl);
    TXTRecordDeallocate(&txtRecord);

    if (ret != kDNSServiceErr_NoError) SoapySDR::logf(
        SOAPY_SDR_ERROR, "DNSServiceRegister() failed %d", ret);
    else DNSServiceProcessResult(_impl->sdRef);
}

/***********************************************************************
 * Implement host discovery
 **********************************************************************/
struct SoapyMDNSBrowseResult
{
    int ipVerRequest;
    std::map<std::string, std::map<int, std::string>> serverURLs;
};

static void getAddrInfoCallback(
    DNSServiceRef sdRef,
    DNSServiceFlags flags,
    uint32_t interfaceIndex,
    DNSServiceErrorType errorCode,
    const char                       *hostname,
    const struct sockaddr            *address,
    uint32_t ttl,
    void                             *context)
{
    auto addrStr = (std::string *)context;

    if (errorCode != kDNSServiceErr_NoError) return SoapySDR::logf(
        SOAPY_SDR_ERROR, "SoapyMDNS getAddrInfoCallback(%s) error: %d", hostname, errorCode);
    *addrStr = SoapyURL(address).getNode();
}

static void resolveReplyCallback(
    DNSServiceRef sdRef,
    DNSServiceFlags flags,
    uint32_t interfaceIndex,
    DNSServiceErrorType errorCode,
    const char                          *fullname,
    const char                          *hosttarget,
    uint16_t port,                                   /* In network byte order */
    uint16_t txtLen,
    const unsigned char                 *txtRecord,
    void                                *context)
{
    auto result = (SoapyMDNSBrowseResult *)context;
    if (errorCode != kDNSServiceErr_NoError) return SoapySDR::logf(
        SOAPY_SDR_ERROR, "SoapyMDNS resolveReplyCallback(%s) error: %d", hosttarget, errorCode);

    //extract uuid
    std::string uuid;
    uint8_t valueLen(0);
    auto uuidPtr = TXTRecordGetValuePtr(txtLen, txtRecord, "uuid", &valueLen);
    if (uuidPtr != nullptr) uuid = std::string((const char *)uuidPtr, valueLen);
    else return SoapySDR::logf(SOAPY_SDR_ERROR, "SoapyMDNS resolve missing uuid record for %s", hosttarget);

    //address lookup
    static const int IP_VERS[] = {SOAPY_REMOTE_IPVER_INET, SOAPY_REMOTE_IPVER_INET6};
    static const DNSServiceProtocol PROTS[] = {kDNSServiceProtocol_IPv4, kDNSServiceProtocol_IPv6};
    const auto service = std::to_string(ntohs(port));
    for (size_t i = 0; i < 2; i++)
    {
        const auto ipVer = IP_VERS[i];
        const auto protocol = PROTS[i];
        if ((ipVer & result->ipVerRequest) == 0) continue;
        std::string addrStr;
        if (DNSServiceGetAddrInfo(
            &sdRef,
            DNSServiceFlags(0),
            interfaceIndex,
            protocol,
            hosttarget,
            &getAddrInfoCallback,
            &addrStr) != kDNSServiceErr_NoError) continue;
        if (DNSServiceProcessResult(sdRef) != kDNSServiceErr_NoError) continue;
        if (addrStr.empty()) continue;
        const auto serverURL = SoapyURL("tcp", addrStr, service).toString();
        SoapySDR::logf(SOAPY_SDR_DEBUG, "SoapyMDNS discovered %s [%s] IPv%d", serverURL.c_str(), uuid.c_str(), ipVer);
        result->serverURLs[uuid][ipVer] = serverURL;
    }
}

static void browseReplyCallback(
    DNSServiceRef sdRef,
    DNSServiceFlags flags,
    uint32_t interfaceIndex,
    DNSServiceErrorType errorCode,
    const char                          *serviceName,
    const char                          *regtype,
    const char                          *replyDomain,
    void                                *context)
{
    char fullname[kDNSServiceMaxDomainName];
    DNSServiceConstructFullName(fullname, serviceName, regtype, replyDomain);
    SoapySDR::logf(SOAPY_SDR_DEBUG, "SoapyMDNS resolving %s...", fullname);

    if (errorCode != kDNSServiceErr_NoError) return SoapySDR::logf(
        SOAPY_SDR_ERROR, "SoapyMDNS browseReplyCallback(#%d, %s) error: %d",
        interfaceIndex, fullname, errorCode);

    auto ret = DNSServiceResolve(
        &sdRef,
        DNSServiceFlags(0),
        interfaceIndex,
        serviceName,
        regtype,
        replyDomain,
        &resolveReplyCallback,
        context);

    if (ret != kDNSServiceErr_NoError) SoapySDR::logf(
        SOAPY_SDR_ERROR, "DNSServiceResolve(#%d, %s) failed %d",
        interfaceIndex, fullname, ret);
    else DNSServiceProcessResult(sdRef);
}

static bool waitDNSServiceRef(DNSServiceRef sdRef, const long timeoutUs)
{
    //get the underlying Unix domain socket for timeout
    auto sock = DNSServiceRefSockFD(sdRef);
    if (sock == -1)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "DNSServiceRefSockFD() failed %d", sock);
        return false;
    }

    struct timeval tv;
    tv.tv_sec = timeoutUs / 1000000;
    tv.tv_usec = timeoutUs % 1000000;

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);

    const int ret = ::select(sock+1, &readfds, NULL, NULL, &tv);
    if (ret == 0) SoapySDR::logf(SOAPY_SDR_DEBUG, "select(DNSServiceRefSockFD()) timeout");
    if (ret == -1) SoapySDR::logf(SOAPY_SDR_ERROR, "select(DNSServiceRefSockFD()) failed %d", errno);
    return ret == 1;
}

std::map<std::string, std::map<int, std::string>> SoapyMDNSEndpoint::getServerURLs(const int ipVer, const long timeoutUs)
{
    SoapyMDNSBrowseResult result;
    result.ipVerRequest = ipVer;
    DNSServiceRef sdRef(nullptr);
    auto ret = DNSServiceBrowse(
        &sdRef,
        DNSServiceFlags(0),
        kDNSServiceInterfaceIndexAny,
        SOAPY_REMOTE_DNSSD_TYPE,
        nullptr, //domain automatic
        &browseReplyCallback,
        &result);

    if (ret != kDNSServiceErr_NoError) SoapySDR::logf(
        SOAPY_SDR_ERROR, "DNSServiceBrowse() failed %d", ret);
    else if (waitDNSServiceRef(sdRef, timeoutUs)) DNSServiceProcessResult(sdRef);
    DNSServiceRefDeallocate(sdRef);
    return result.serverURLs;
}
