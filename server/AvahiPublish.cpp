// Copyright (c) 2018-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyRemoteDefs.hpp"
#include "AvahiPublish.hpp"
#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/error.h>
#include <cstdlib> //atoi
#include <iostream>

static void clientCallback(AvahiClient *s, AvahiClientState state, void* userdata)
{
    std::cout << "clientCallback!\n";
}

static void groupCallback(AvahiEntryGroup *g, AvahiEntryGroupState state, void* userdata)
{
    std::cout << "groupCallback!\n";
}

AvahiPublish::AvahiPublish(void):
    simplePoll(nullptr),
    poll(nullptr),
    client(nullptr),
    group(nullptr)
{
    simplePoll = avahi_simple_poll_new();
    if (simplePoll == nullptr)
    {
        std::cerr << "AvahiPublish avahi_simple_poll_new() failed" << std::endl;
        return;
    }

    poll = avahi_simple_poll_get(simplePoll);
    if (poll == nullptr)
    {
        std::cerr << "AvahiPublish avahi_simple_poll_get() failed" << std::endl;
        return;
    }

    int error(0);
    client = avahi_client_new(poll, AVAHI_CLIENT_NO_FAIL, &clientCallback, this, &error);
    if (client == nullptr or error != 0)
    {
        std::cerr << "AvahiPublish avahi_client_new() failed: " << avahi_strerror(error) << std::endl;
        return;
    }

    std::cout << "  Avahi version:     " << avahi_client_get_version_string(client) << std::endl;
    std::cout << "  Avahi hostname:    " << avahi_client_get_host_name(client) << std::endl;
    std::cout << "  Avahi domain:      " << avahi_client_get_domain_name(client) << std::endl;
    std::cout << "  Avahi FQDN:        " << avahi_client_get_host_name_fqdn(client) << std::endl;

    group = avahi_entry_group_new(client, &groupCallback, this);
    if (group == nullptr or error != 0)
    {
        std::cerr << "AvahiPublish avahi_entry_group_new() failed" << std::endl;
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
        atoi(SOAPY_REMOTE_DEFAULT_SERVICE),
        nullptr);
    if (ret != 0)
    {
        std::cerr << "AvahiPublish avahi_entry_group_add_service() failed: " << avahi_strerror(ret) << std::endl;
        return;
    }

    ret = avahi_entry_group_commit(group);
    if (ret != 0)
    {
        std::cerr << "AvahiPublish avahi_entry_group_commit() failed: " << avahi_strerror(ret) << std::endl;
        return;
    }
}

AvahiPublish::~AvahiPublish(void)
{
    if (group != nullptr) avahi_entry_group_free(group);
    if (client != nullptr) avahi_client_free(client);
    if (simplePoll != nullptr) avahi_simple_poll_free(simplePoll);
}
