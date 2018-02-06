// Copyright (c) 2018-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <SoapySDR/Logger.hpp>
#include "SoapyRemoteDefs.hpp"
#include "SoapyURLUtils.hpp"
#include "SoapyDNSSD.hpp"
#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-client/lookup.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <cstdlib> //atoi
#include <thread>
#include <map>

#define SOAPY_REMOTE_DNSSD_NAME "SoapyRemote"

#define SOAPY_REMOTE_DNSSD_TYPE "_soapy._tcp"

static int ipVerToAvahiProtocol(const int ipVer)
{
    int protocol = AVAHI_PROTO_UNSPEC;
    if (ipVer == SOAPY_REMOTE_IPVER_UNSPEC) protocol = AVAHI_PROTO_UNSPEC;
    if (ipVer == SOAPY_REMOTE_IPVER_INET)   protocol = AVAHI_PROTO_INET;
    if (ipVer == SOAPY_REMOTE_IPVER_INET6)  protocol = AVAHI_PROTO_INET6;
    return protocol;
}

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
        SoapySDR::logf(SOAPY_SDR_DEBUG, "Avahi client running...");
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
        SoapySDR::logf(SOAPY_SDR_DEBUG, "Avahi group established...");
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

bool SoapyDNSSD::status(void)
{
    return avahi_client_get_state(_impl->client) != AVAHI_CLIENT_FAILURE;
}

void SoapyDNSSD::registerService(const std::string &uuid, const std::string &service, const int ipVer)
{
    auto &client = _impl->client;
    auto &group = _impl->group;
    group = avahi_entry_group_new(client, &SoapyDNSSDImpl::groupCallback, this);
    if (group == nullptr)
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "avahi_entry_group_new() failed");
        return;
    }

    auto txt = avahi_string_list_add_pair(nullptr, "uuid", uuid.c_str());
    int ret = avahi_entry_group_add_service_strlst(
        group,
        AVAHI_IF_UNSPEC,
        ipVerToAvahiProtocol(ipVer),
        AvahiPublishFlags(0),
        SOAPY_REMOTE_DNSSD_NAME,
        SOAPY_REMOTE_DNSSD_TYPE,
        nullptr,
        nullptr,
        atoi(service.c_str()),
        txt);
    avahi_string_list_free(txt);

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

/***********************************************************************
 * Implement host discovery
 **********************************************************************/
struct SoapyDNSSDBrowseResults
{
    SoapyDNSSDBrowseResults(void):
        resolversInFlight(0), browseComplete(false){}
    bool done(void) {return resolversInFlight == 0 and browseComplete;}
    std::map<std::string, std::map<int, std::string>> uuidToUrl;
    size_t resolversInFlight;
    bool browseComplete;
    void append(
        const int ipVer,
        const std::string &uuid,
        const std::string &host,
        uint16_t port)
    {
        if (uuid.empty()) return;
        const auto serverURL = SoapyURL("tcp", host, std::to_string(port)).toString();
        uuidToUrl[uuid][ipVer] = serverURL;
        SoapySDR::logf(SOAPY_SDR_DEBUG, "SoapyDNSSD discovered %s [%s]", serverURL.c_str(), uuid.c_str());
    }
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
    void *userdata)
{
    auto &results = *reinterpret_cast<SoapyDNSSDBrowseResults*>(userdata);

    if (event == AVAHI_RESOLVER_FOUND and address != nullptr)
    {
        //extract address
        char addrStr[AVAHI_ADDRESS_STR_MAX];
        avahi_address_snprint(addrStr, sizeof(addrStr), address);

        //extract key/value pairs
        std::map<std::string, std::string> fields;
        for (; txt != nullptr; txt = txt->next)
        {
            char *key(nullptr), *value(nullptr); size_t size(0);
            avahi_string_list_get_pair(txt, &key, &value, &size);
            if (key == nullptr or value == nullptr) continue;
            fields[key] = std::string(value, size);
            avahi_free(key);
            avahi_free(value);
        }

        //append the result based on protocol
        if (protocol == AVAHI_PROTO_INET)
        {
            results.append(SOAPY_REMOTE_IPVER_INET, fields["uuid"], addrStr, port);
        }
        if (protocol == AVAHI_PROTO_INET6)
        {
            const auto ifaceStr = "%" + std::to_string(interface);
            results.append(SOAPY_REMOTE_IPVER_INET6, fields["uuid"], addrStr + ifaceStr, port);
        }
    }

    //cleanup
    results.resolversInFlight--;
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
    auto &results = *reinterpret_cast<SoapyDNSSDBrowseResults*>(userdata);
    auto c = avahi_service_browser_get_client(b);

    switch (event) {
    case AVAHI_BROWSER_FAILURE:
        SoapySDR::logf(SOAPY_SDR_ERROR, "Avahi browser error: %s", avahi_strerror(avahi_client_errno(c)));
        results.browseComplete = true;
        results.resolversInFlight = 0;
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
        else results.resolversInFlight++; //resolver is freed by the callback
        break;

    //don't care about removals, browser lifetime is short
    case AVAHI_BROWSER_REMOVE: break;

    //flags the results when the cache is exhausted (or all for now)
    case AVAHI_BROWSER_ALL_FOR_NOW:
    case AVAHI_BROWSER_CACHE_EXHAUSTED:
        results.browseComplete = true;
        break;
    }
}

std::map<std::string, std::map<int, std::string>> SoapyDNSSD::getServerURLs(const int ipVer)
{
    //create a new service browser with results structure and callback
    SoapyDNSSDBrowseResults results;
    auto browser = avahi_service_browser_new(
        _impl->client,
        AVAHI_IF_UNSPEC,
        ipVerToAvahiProtocol(ipVer),
        SOAPY_REMOTE_DNSSD_TYPE,
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
    while (not results.done())
    {
        avahi_simple_poll_iterate(_impl->simplePoll, -1);
    }

    //cleanup
    avahi_service_browser_free(browser);
    return results.uuidToUrl;
}
