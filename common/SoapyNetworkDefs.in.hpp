// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

// ** This header should be included first, to avoid compile errors.
// ** At least in the case of the windows header files.

// This header helps to abstract network differences between platforms.
// Including the correct headers for various network APIs.
// And providing various typedefs and definitions when missing.

#pragma once

/***********************************************************************
 * Windows socket headers
 **********************************************************************/
#cmakedefine HAS_WINSOCK2_H
#ifdef HAS_WINSOCK2_H
#include <winsock2.h> //htonll
#endif //HAS_WINSOCK2_H

#cmakedefine HAS_WS2TCPIP_H
#ifdef HAS_WS2TCPIP_H
#include <ws2tcpip.h> //addrinfo
typedef int socklen_t;
#endif //HAS_WS2TCPIP_H

/***********************************************************************
 * unix socket headers
 **********************************************************************/
#cmakedefine HAS_UNISTD_H
#ifdef HAS_UNISTD_H
#include <unistd.h> //close
#define closesocket close
#endif //HAS_UNISTD_H

#cmakedefine HAS_NETDB_H
#ifdef HAS_NETDB_H
#include <netdb.h> //addrinfo
#endif //HAS_NETDB_H

#cmakedefine HAS_NETINET_IN_H
#ifdef HAS_NETINET_IN_H
#include <netinet/in.h>
#endif //HAS_NETINET_IN_H

#cmakedefine HAS_NETINET_TCP_H
#ifdef HAS_NETINET_TCP_H
#include <netinet/tcp.h>
#endif //HAS_NETINET_TCP_H

#cmakedefine HAS_SYS_TYPES_H
#ifdef HAS_SYS_TYPES_H
#include <sys/types.h>
#endif //HAS_SYS_TYPES_H

#cmakedefine HAS_SYS_SOCKET_H
#ifdef HAS_SYS_SOCKET_H
#include <sys/socket.h>
#endif //HAS_SYS_SOCKET_H

/***********************************************************************
 * htonll and ntohll for GCC
 **********************************************************************/
#ifdef __GNUC__
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define htonll(x) __builtin_bswap64(x)
    #define ntohll(x) __builtin_bswap64(x)
#else //big endian
    #define htonll(x) (x)
    #define ntohll(x) (x)
#endif //little endian
#endif //__GNUC__
