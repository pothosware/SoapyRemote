// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "ThreadPrioHelper.hpp"
#include <windows.h>

std::string setThreadPrio(const double prio)
{
    int nPriority(THREAD_PRIORITY_NORMAL);

    if (prio > 0)
    {
        if      (prio > +0.75) nPriority = THREAD_PRIORITY_TIME_CRITICAL;
        else if (prio > +0.50) nPriority = THREAD_PRIORITY_HIGHEST;
        else if (prio > +0.25) nPriority = THREAD_PRIORITY_ABOVE_NORMAL;
        else                   nPriority = THREAD_PRIORITY_NORMAL;
    }
    else
    {
        if      (prio < -0.75) nPriority = THREAD_PRIORITY_IDLE;
        else if (prio < -0.50) nPriority = THREAD_PRIORITY_LOWEST;
        else if (prio < -0.25) nPriority = THREAD_PRIORITY_BELOW_NORMAL;
        else                   nPriority = THREAD_PRIORITY_NORMAL;
    }

    if (SetThreadPriority(GetCurrentThread(), nPriority)) return "";
    return "SetThreadPriority() fail";
}
