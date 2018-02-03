// Copyright (c) 2018-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyRemoteDefs.hpp"
#include "DNSSDPublish.hpp"
#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/error.h>
#include <cstdlib> //atoi
#include <thread>
#include <iostream>

struct DNSSDPublishImpl
{
    DNSSDPublishImpl(void);
    ~DNSSDPublishImpl(void);
    AvahiSimplePoll *simplePoll;
    std::thread *pollThread;
    AvahiClient *client;
    AvahiEntryGroup *group;
    void setupService(AvahiClient *c);
    static void clientCallback(AvahiClient *c, AvahiClientState state, void *userdata);
    static void groupCallback(AvahiEntryGroup *g, AvahiEntryGroupState state, void *userdata);
};

void DNSSDPublishImpl::setupService(AvahiClient *c)
{
    if (group == nullptr) group = avahi_entry_group_new(c, &groupCallback, this);
    if (group == nullptr)
    {
        std::cerr << "avahi_entry_group_new() failed" << std::endl;
        return;
    }

    if (not avahi_entry_group_is_empty(group)) return;

    int ret = avahi_entry_group_add_service(
        group,
        AVAHI_IF_UNSPEC,
        AVAHI_PROTO_UNSPEC,
        AvahiPublishFlags(0),
        "SoapySDRServer",
        "_soapy._tcp",
        nullptr,
        nullptr,
        atoi(SOAPY_REMOTE_DEFAULT_SERVICE),
        nullptr);
    if (ret != 0)
    {
        std::cerr << "avahi_entry_group_add_service() failed: " << avahi_strerror(ret) << std::endl;
        return;
    }

    ret = avahi_entry_group_commit(group);
    if (ret != 0)
    {
        std::cerr << "avahi_entry_group_commit() failed: " << avahi_strerror(ret) << std::endl;
        return;
    }
}

void DNSSDPublishImpl::clientCallback(AvahiClient *c, AvahiClientState state, void *userdata)
{
    auto impl = (DNSSDPublishImpl*)userdata;
    switch (state)
    {
    case AVAHI_CLIENT_S_RUNNING: //success
        std::cout << "Avahi client running..." << std::endl;
        impl->setupService(c);
        break;

    case AVAHI_CLIENT_FAILURE: //error
        std::cerr << "Avahi client error " << avahi_strerror(avahi_client_errno(c)) << std::endl;
        if (impl->simplePoll != nullptr) avahi_simple_poll_quit(impl->simplePoll);
        break;

    case AVAHI_CLIENT_S_COLLISION:
    case AVAHI_CLIENT_S_REGISTERING: //clear group and re-establish when running
        std::cerr << "Avahi client reset " << avahi_strerror(avahi_client_errno(c)) << std::endl;
        if (impl->group != nullptr) avahi_entry_group_reset(impl->group);
        break;

    case AVAHI_CLIENT_CONNECTING:
        break;
    }
}

void DNSSDPublishImpl::groupCallback(AvahiEntryGroup *g, AvahiEntryGroupState state, void* userdata)
{
    auto impl = (DNSSDPublishImpl*)userdata;
    const auto c = avahi_entry_group_get_client(g);
    switch (state)
    {
    case AVAHI_ENTRY_GROUP_ESTABLISHED: //success
        std::cout << "Avahi group established..." << std::endl;
        break;

    case AVAHI_ENTRY_GROUP_COLLISION: //don't run if the group is already registered
    case AVAHI_ENTRY_GROUP_FAILURE: //error
        std::cerr << "Avahi group error " << avahi_strerror(avahi_client_errno(c)) << std::endl;
        if (impl->simplePoll != nullptr) avahi_simple_poll_quit(impl->simplePoll);
        break;

    case AVAHI_ENTRY_GROUP_UNCOMMITED:
    case AVAHI_ENTRY_GROUP_REGISTERING:
        break;
    }
}

DNSSDPublishImpl::DNSSDPublishImpl(void):
    simplePoll(nullptr),
    pollThread(nullptr),
    client(nullptr),
    group(nullptr)
{
    simplePoll = avahi_simple_poll_new();
    if (simplePoll == nullptr)
    {
        std::cerr << "avahi_simple_poll_new() failed" << std::endl;
        return;
    }

    int error(0);
    client = avahi_client_new(avahi_simple_poll_get(simplePoll), AVAHI_CLIENT_NO_FAIL, &clientCallback, this, &error);
    if (client == nullptr or error != 0)
    {
        std::cerr << "avahi_client_new() failed: " << avahi_strerror(error) << std::endl;
        return;
    }

    pollThread = new std::thread(&avahi_simple_poll_loop, simplePoll);
}

DNSSDPublishImpl::~DNSSDPublishImpl(void)
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

DNSSDPublish::DNSSDPublish(void):
    _impl(new DNSSDPublishImpl())
{
    std::cout << "  Avahi version:     " << avahi_client_get_version_string(_impl->client) << std::endl;
    std::cout << "  Avahi hostname:    " << avahi_client_get_host_name(_impl->client) << std::endl;
    std::cout << "  Avahi domain:      " << avahi_client_get_domain_name(_impl->client) << std::endl;
    std::cout << "  Avahi FQDN:        " << avahi_client_get_host_name_fqdn(_impl->client) << std::endl;
}

DNSSDPublish::~DNSSDPublish(void)
{
    delete _impl;
}

void DNSSDPublish::maintenance(void)
{
    if (_impl->client == nullptr) return;
    if (avahi_client_get_state(_impl->client) != AVAHI_CLIENT_FAILURE) return;
    delete _impl;
    _impl = new DNSSDPublishImpl();
}
