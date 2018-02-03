// Copyright (c) 2018-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once

struct AvahiSimplePoll;
struct AvahiPoll;
struct AvahiClient;
struct AvahiEntryGroup;

class AvahiPublish
{
public:
    AvahiPublish(void);
    ~AvahiPublish(void);
private:
    struct AvahiSimplePoll *simplePoll;
    const struct AvahiPoll *poll;
    struct AvahiClient *client;
    struct AvahiEntryGroup *group;
};
