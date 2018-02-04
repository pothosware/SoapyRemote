// Copyright (c) 2018-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <SoapySDR/Logger.hpp>
#include "SoapyURLUtils.hpp"
#include "SoapyDNSSD.hpp"
#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-client/lookup.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/error.h>
#include <cstdlib> //atoi
#include <thread>
#include <map>
#include <iostream>

/***********************************************************************
 * Storage for avahi client
 **********************************************************************/
struct SoapyDNSSDImpl
{
    SoapyDNSSDImpl(void);
    ~SoapyDNSSDImpl(void);
    AvahiSimplePoll *simplePoll;
    std::thread *pollThread;
    AvahiClient *client;
    AvahiEntryGroup *group;
    static void clientCallback(AvahiClient *c, AvahiClientState state, void *userdata);
    static void groupCallback(AvahiEntryGroup *g, AvahiEntryGroupState state, void *userdata);
};

SoapyDNSSDImpl::SoapyDNSSDImpl(void):
    simplePoll(nullptr),
    pollThread(nullptr),
    client(nullptr),
    group(nullptr)
{
    simplePoll = avahi_simple_poll_new();
    if (simplePoll == nullptr)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "avahi_simple_poll_new() failed");
        return;
    }

    int error(0);
    client = avahi_client_new(avahi_simple_poll_get(simplePoll), AVAHI_CLIENT_NO_FAIL, &SoapyDNSSDImpl::clientCallback, this, &error);
    if (client == nullptr or error != 0)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "avahi_client_new() failed: %s", avahi_strerror(error));
        return;
    }
}

SoapyDNSSDImpl::~SoapyDNSSDImpl(void)
{
    if (simplePoll != nullptr) avahi_simple_poll_quit(simplePoll);
    if (pollThread != nullptr)
    {
        pollThread->join();
        delete pollThread;
    }
    if (group != nullptr) avahi_entry_group_free(group);
    if (client != nullptr) avahi_client_free(client);
    if (simplePoll != nullptr) avahi_simple_poll_free(simplePoll);
}

void SoapyDNSSDImpl::clientCallback(AvahiClient *c, AvahiClientState state, void *userdata)
{
    auto impl = (SoapyDNSSDImpl*)userdata;
    switch (state)
    {
    case AVAHI_CLIENT_S_RUNNING: //success
        SoapySDR::logf(SOAPY_SDR_INFO, "Avahi client running...");
        break;

    case AVAHI_CLIENT_S_COLLISION:
    case AVAHI_CLIENT_FAILURE: //error
        SoapySDR::logf(SOAPY_SDR_ERROR, "Avahi client failure: %s",  avahi_strerror(avahi_client_errno(c)));
        if (impl->simplePoll != nullptr) avahi_simple_poll_quit(impl->simplePoll);
        break;

    case AVAHI_CLIENT_S_REGISTERING:
    case AVAHI_CLIENT_CONNECTING:
        break;
    }
}

void SoapyDNSSDImpl::groupCallback(AvahiEntryGroup *g, AvahiEntryGroupState state, void* userdata)
{
    auto impl = (SoapyDNSSDImpl*)userdata;
    const auto c = avahi_entry_group_get_client(g);
    switch (state)
    {
    case AVAHI_ENTRY_GROUP_ESTABLISHED: //success
        SoapySDR::logf(SOAPY_SDR_INFO, "Avahi group established...");
        break;

    case AVAHI_ENTRY_GROUP_COLLISION:
    case AVAHI_ENTRY_GROUP_FAILURE: //error
        SoapySDR::logf(SOAPY_SDR_ERROR, "Avahi group failure: %s",  avahi_strerror(avahi_client_errno(c)));
        if (impl->simplePoll != nullptr) avahi_simple_poll_quit(impl->simplePoll);
        break;

    case AVAHI_ENTRY_GROUP_UNCOMMITED:
    case AVAHI_ENTRY_GROUP_REGISTERING:
        break;
    }
}

/***********************************************************************
 * SoapyDNSSD interface hooks
 **********************************************************************/
std::shared_ptr<SoapyDNSSD> SoapyDNSSD::getInstance(void)
{
    static std::shared_ptr<SoapyDNSSD> instance(new SoapyDNSSD());
    return instance;
}

SoapyDNSSD::SoapyDNSSD(void):
    _impl(new SoapyDNSSDImpl())
{
    return;
}

SoapyDNSSD::~SoapyDNSSD(void)
{
    if (_impl != nullptr) delete _impl;
}

void SoapyDNSSD::printInfo(void)
{
    //summary of avahi client connection for server logging
    SoapySDR::logf(SOAPY_SDR_INFO, "Avahi version:  %s", avahi_client_get_version_string(_impl->client));
    SoapySDR::logf(SOAPY_SDR_INFO, "Avahi hostname: %s", avahi_client_get_host_name(_impl->client));
    SoapySDR::logf(SOAPY_SDR_INFO, "Avahi domain:   %s", avahi_client_get_domain_name(_impl->client));
    SoapySDR::logf(SOAPY_SDR_INFO, "Avahi FQDN:     %s", avahi_client_get_host_name_fqdn(_impl->client));
}

void SoapyDNSSD::registerService(const std::string &uuid, const std::string &service)
{
    this->_uuid = uuid;
    this->_service = service;

    auto &client = _impl->client;
    auto &group = _impl->group;
    group = avahi_entry_group_new(client, &SoapyDNSSDImpl::groupCallback, this);
    if (group == nullptr)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "avahi_entry_group_new() failed");
        return;
    }

    int ret = avahi_entry_group_add_service(
        group,
        AVAHI_IF_UNSPEC,
        AVAHI_PROTO_UNSPEC,
        AvahiPublishFlags(0),
        "SoapySDRServer",
        "_soapy._tcp",
        nullptr,
        nullptr,
        atoi(service.c_str()),
        uuid.c_str(),
        nullptr);

    if (ret != 0)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "avahi_entry_group_add_service() failed: %s", avahi_strerror(ret));
        return;
    }

    ret = avahi_entry_group_commit(group);
    if (ret != 0)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "avahi_entry_group_commit() failed: %s", avahi_strerror(ret));
        return;
    }

    _impl->pollThread = new std::thread(&avahi_simple_poll_loop, _impl->simplePoll);
}

void SoapyDNSSD::maintenance(void)
{
    if (_impl->client == nullptr) return;
    if (avahi_client_get_state(_impl->client) != AVAHI_CLIENT_FAILURE) return;
    delete _impl;
    _impl = new SoapyDNSSDImpl();
    this->registerService(_uuid, _service);
}

/***********************************************************************
 * Implement host discovery
 **********************************************************************/
struct SoapyDNSSDBrowseResults
{
    SoapyDNSSDBrowseResults(void):
        resolversInFlight(0), browseComplete(false){}
    bool done(void) {return resolversInFlight == 0 and browseComplete;}
    std::map<std::string, std::map<int, std::string>> uuidToIpVerToUrl;
    size_t resolversInFlight;
    bool browseComplete;
};

void resolverCallback(
    AvahiServiceResolver *r,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiResolverEvent event,
    const char */*name*/,
    const char */*type*/,
    const char */*domain*/,
    const char */*host_name*/,
    const AvahiAddress *address,
    uint16_t port,
    AvahiStringList *txt,
    AvahiLookupResultFlags /*flags*/,
    void *userdata) {

    auto results = (SoapyDNSSDBrowseResults*)userdata;

    char addrStr[AVAHI_ADDRESS_STR_MAX];
    avahi_address_snprint(addrStr, sizeof(addrStr), address);

    //extract uuid
    std::string uuid;
    if (txt != nullptr) uuid = std::string((const char *)txt->text, txt->size);

    if (event == AVAHI_RESOLVER_FOUND and protocol == AVAHI_PROTO_INET and not uuid.empty())
    {
        const SoapyURL url("tcp", addrStr, std::to_string(port));
        results->uuidToIpVerToUrl[uuid][4] = url.toString();
    }

    if (event == AVAHI_RESOLVER_FOUND and protocol == AVAHI_PROTO_INET6 and not uuid.empty())
    {
        const auto ifaceStr = "%" + std::to_string(interface);
        const SoapyURL url("tcp", addrStr + ifaceStr, std::to_string(port));
        results->uuidToIpVerToUrl[uuid][6] = url.toString();
    }

    results->resolversInFlight--;
    avahi_service_resolver_free(r);
}

void browserCallback(
    AvahiServiceBrowser *b,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiBrowserEvent event,
    const char *name,
    const char *type,
    const char *domain,
    AvahiLookupResultFlags /*flags*/,
    void *userdata)
{
    auto results = (SoapyDNSSDBrowseResults*)userdata;
    auto c = avahi_service_browser_get_client(b);

    switch (event) {
    case AVAHI_BROWSER_FAILURE:
        SoapySDR::logf(SOAPY_SDR_ERROR, "Avahi browser error: %s", avahi_strerror(avahi_client_errno(c)));
        results->browseComplete = true;
        results->resolversInFlight = 0;
        return;

    case AVAHI_BROWSER_NEW:
        if (avahi_service_resolver_new(
            c,
            interface,
            protocol,
            name,
            type,
            domain,
            AVAHI_PROTO_UNSPEC,
            AvahiLookupFlags(0),
            resolverCallback,
            userdata) == nullptr
        )
            SoapySDR::logf(SOAPY_SDR_ERROR, "avahi_service_resolver_new() failed: %s", avahi_strerror(avahi_client_errno(c)));
        else results->resolversInFlight++; //resolver is freed by the callback
        break;

    //don't care about removals, browser lifetime is short
    case AVAHI_BROWSER_REMOVE: break;

    //flags the results when the cache is exhausted (or all for now)
    case AVAHI_BROWSER_ALL_FOR_NOW:
    case AVAHI_BROWSER_CACHE_EXHAUSTED:
        results->browseComplete = true;
        break;
    }
}

std::vector<std::string> SoapyDNSSD::getServerURLs(const int ipVer, const bool only)
{
    //create a new service browser with results structure and callback
    SoapyDNSSDBrowseResults results;
    auto browser = avahi_service_browser_new(
        _impl->client,
        AVAHI_IF_UNSPEC,
        AVAHI_PROTO_UNSPEC,
        "_soapy._tcp",
        nullptr,
        AvahiLookupFlags(0),
        &browserCallback,
        &results);

    if (browser == nullptr)
    {
        int error = avahi_client_errno(_impl->client);
        SoapySDR::logf(SOAPY_SDR_ERROR, "avahi_service_browser_new() failed: %s", avahi_strerror(error));
        return {};
    }

    //run the handler until the results are completed
    while(not results.done())
    {
        avahi_simple_poll_iterate(_impl->simplePoll, -1);
    }

    //cleanup
    avahi_service_browser_free(browser);

    //extract urls based on input filter
    std::vector<std::string> urlsOut;
    for (const auto &pair : results.uuidToIpVerToUrl)
    {
        const auto &ipVerToUrl = pair.second;

        //is there a matching URL for this IP version?
        auto itPref = ipVerToUrl.find(ipVer);
        if (itPref != ipVerToUrl.end())
        {
            urlsOut.push_back(itPref->second);
        }

        //otherwise use the available entry when not 'only' mode
        else if (not only and not ipVerToUrl.empty())
        {
            urlsOut.push_back(ipVerToUrl.begin()->second);
        }
    }
    return urlsOut;
}