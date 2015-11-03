// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <cctype> //isdigit
#include <string>
#include <cstddef>

//! convert stream format string to element size in bytes
static inline size_t formatToSize(const std::string &format)
{
    size_t size = 0;
    size_t isComplex = false;
    for (const char ch : format)
    {
        if (ch == 'C') isComplex = true;
        if (std::isdigit(ch)) size = (size*10) + size_t(ch-'0');
    }
    if (isComplex) size *= 2;
    return size / 8; //bits to bytes
}
