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

/***********************************************************************
 * Constant definitions
 **********************************************************************/
//! The default bind port for the remote server
#define SOAPY_REMOTE_DEFAULT_SERVICE "55132"

//! Use this key with device arguments to specify remote server url
#define SOAPY_REMOTE_KWARG_KEY "remote"

//! Use this magic stop key in the server to prevent infinite loops
#define SOAPY_REMOTE_KWARG_STOP "soapy_remote_no_deeper"

//! Use this timeout to poll for socket accept in server listener
#define SOAPY_REMOTE_ACCEPT_TIMEOUT_US (100*1000) //10 ms

//! Backlog count for the server socket listen
#define SOAPY_REMOTE_LISTEN_BACKLOG 100

/***********************************************************************
 * RPC structures and constants
 **********************************************************************/
//major, minor, patch when this was last updated
//bump the version number when changes are made
static const unsigned int SoapyRPCVersion = 0x000300;

enum SoapyRemoteTypes
{
    SOAPY_REMOTE_CHAR            = 0,
    SOAPY_REMOTE_BOOL            = 1,
    SOAPY_REMOTE_INT32           = 2,
    SOAPY_REMOTE_INT64           = 3,
    SOAPY_REMOTE_FLOAT64         = 4,
    SOAPY_REMOTE_COMPLEX128      = 5,
    SOAPY_REMOTE_STRING          = 6,
    SOAPY_REMOTE_RANGE           = 7,
    SOAPY_REMOTE_RANGE_LIST      = 8,
    SOAPY_REMOTE_STRING_LIST     = 9,
    SOAPY_REMOTE_FLOAT64_LIST    = 10,
    SOAPY_REMOTE_KWARGS          = 11,
    SOAPY_REMOTE_KWARGS_LIST     = 12,
    SOAPY_REMOTE_EXCEPTION       = 13,
    SOAPY_REMOTE_VOID            = 14,
    SOAPY_REMOTE_CALL            = 15,
    SOAPY_REMOTE_TYPE_MAX        = 16,
};

enum SoapyRemoteCalls
{
    SOAPY_REMOTE_FIND            = 0,
    SOAPY_REMOTE_MAKE            = 1,
    SOAPY_REMOTE_UNMAKE          = 2,

    SOAPY_REMOTE_GET_DRIVER_KEY      = 10,
    SOAPY_REMOTE_GET_HARDWARE_KEY    = 11,
    SOAPY_REMOTE_GET_HARDWARE_INFO   = 12,

    SOAPY_REMOTE_SET_FRONTEND_MAPPING      = 20,
    SOAPY_REMOTE_GET_FRONTEND_MAPPING      = 21,
    SOAPY_REMOTE_GET_NUM_CHANNELS          = 22,
    SOAPY_REMOTE_GET_FULL_DUPLEX           = 23,
};

#define SOAPY_PACKET_WORD32(str) \
    ((unsigned int)(str[0]) << 24) | \
    ((unsigned int)(str[1]) << 16) | \
    ((unsigned int)(str[2]) << 8) | \
    ((unsigned int)(str[3]) << 0)

static const unsigned int SoapyRPCHeaderWord = SOAPY_PACKET_WORD32("SRPC");
static const unsigned int SoapyRPCTrailerWord = SOAPY_PACKET_WORD32("CPRS");

struct SoapyRPCHeader
{
    unsigned int headerWord; //!< header word to identify this protocol
    unsigned int version; //!< version number for protocol compatibility
    unsigned int length; //!< complete packet length in bytes
};

struct SoapyRPCTrailer
{
    unsigned int trailerWord; //!< trailer word to identify this protocol
};
