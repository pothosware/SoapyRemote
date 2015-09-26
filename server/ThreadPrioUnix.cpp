// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "ThreadPrioHelper.hpp"
#include <cstring> //memset, strerror
#include <cerrno> //errno
#include <sched.h>

#ifdef __APPLE__
#include <thread>
#endif

std::string setThreadPrio(const double prio)
{
    //no negative priorities supported on this OS
    if (prio <= 0.0) return "";

    //determine priority bounds
    const int policy(SCHED_RR);
    const int maxPrio = sched_get_priority_max(policy);
    if (maxPrio < 0) return strerror(errno);
    const int minPrio = sched_get_priority_min(policy);
    if (minPrio < 0) return strerror(errno);

    //set realtime priority and prio number
    struct sched_param param;
    std::memset(&param, 0, sizeof(param));
    param.sched_priority = minPrio + int(prio * (maxPrio-minPrio));

#ifdef __APPLE__
    pthread_t tID = pthread_self();  // ID of this thread
    if (pthread_setschedparam(tID, policy, &param) != 0) return strerror(errno);
#else
    if (sched_setscheduler(0, policy, &param) != 0) return strerror(errno);
#endif

    return "";
}
