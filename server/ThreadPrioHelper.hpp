// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include <string>

/*! Set the thread priority:
 * -1.0 (low), 0.0 (normal), and 1.0 (high)
 * Return a empty string on success,
 * otherwise an error message.
 */
std::string setThreadPrio(const double prio);
