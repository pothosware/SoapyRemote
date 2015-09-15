// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <SoapySDR/Config.hpp>

/***********************************************************************
 * API export defines
 **********************************************************************/
#ifdef SOAPY_REMOTE_DLL // defined if SOAPY is compiled as a DLL
  #ifdef SOAPY_REMOTE_DLL_EXPORTS // defined if we are building the SOAPY DLL (instead of using it)
    #define SOAPY_REMOTE_API SOAPY_SDR_HELPER_DLL_EXPORT
  #else
    #define SOAPY_REMOTE_API SOAPY_SDR_HELPER_DLL_IMPORT
  #endif // SOAPY_REMOTE_DLL_EXPORTS
  #define SOAPY_REMOTE_LOCAL SOAPY_SDR_HELPER_DLL_LOCAL
#else // SOAPY_REMOTE_DLL is not defined: this means SOAPY is a static lib.
  #define SOAPY_REMOTE_API SOAPY_SDR_HELPER_DLL_EXPORT
#endif // SOAPY_REMOTE_DLL
