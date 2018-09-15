// Copyright (c) 2018-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

//----------------------------------------------------------
//-- https://docs.microsoft.com/en-us/windows/desktop/api/iphlpapi/nf-iphlpapi-getadaptersaddresses
//----------------------------------------------------------

#include "SoapyIfAddrs.hpp"
#include "SoapySocketDefs.hpp"
#include "SoapyURLUtils.hpp"
#include <ciso646>
#include <iostream>
#include <vector>
#include <map>

#include <winsock2.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <stdlib.h>

// Link with Iphlpapi.lib
#pragma comment(lib, "IPHLPAPI.lib")

#define WORKING_BUFFER_SIZE 15000
#define MAX_TRIES 3

#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))

std::vector<SoapyIfAddr> listSoapyIfAddrs(void)
{
    std::vector<SoapyIfAddr> result;

    //nothing but simplicity https://msdn.microsoft.com/en-us/library/aa365915.aspx
    DWORD dwSize = 0;
    DWORD dwRetVal = 0;

    unsigned int i = 0;

    // Set the flags to pass to GetAdaptersAddresses
    ULONG flags = GAA_FLAG_INCLUDE_PREFIX;

    // default to unspecified address family (both)
    ULONG family = AF_UNSPEC;

    LPVOID lpMsgBuf = NULL;

    PIP_ADAPTER_ADDRESSES pAddresses = NULL;
    ULONG outBufLen = 0;
    ULONG Iterations = 0;

    PIP_ADAPTER_ADDRESSES pCurrAddresses = NULL;

    // Allocate a 15 KB buffer to start with.
    outBufLen = WORKING_BUFFER_SIZE;

    do {

        pAddresses = (IP_ADAPTER_ADDRESSES *) MALLOC(outBufLen);
        if (pAddresses == NULL) {
            printf
                ("Memory allocation failed for IP_ADAPTER_ADDRESSES struct\n");
            return result;
        }

        dwRetVal =
            GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen);

        if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
            FREE(pAddresses);
            pAddresses = NULL;
        } else {
            break;
        }

        Iterations++;

    } while ((dwRetVal == ERROR_BUFFER_OVERFLOW) && (Iterations < MAX_TRIES));

    if (dwRetVal == NO_ERROR) {
        // If successful, output some information from the data we received
        pCurrAddresses = pAddresses;
        while (pCurrAddresses) {

            SoapyIfAddr ifAddr;
            ifAddr.isUp = pCurrAddresses->OperStatus == IfOperStatusUp;
            ifAddr.isLoopback = pCurrAddresses->IfType == IF_TYPE_SOFTWARE_LOOPBACK;
            ifAddr.isMulticast = pCurrAddresses->NoMulticast == 0;
            ifAddr.ethno = pCurrAddresses->Ipv6IfIndex;
            ifAddr.name = pCurrAddresses->AdapterName;

            for (auto addr_i = pCurrAddresses->FirstUnicastAddress; addr_i != NULL; addr_i = addr_i->Next)
            {
                const auto a = addr_i->Address.lpSockaddr;
                switch(a->sa_family)
                {
                case AF_INET: ifAddr.ipVer = 4; break;
                case AF_INET6: ifAddr.ipVer = 6; break;
                default: break;
                }
                if (ifAddr.ipVer == 0) continue;
                ifAddr.addr = SoapyURL(a).getNode();
                result.push_back(ifAddr);
            }

            pCurrAddresses = pCurrAddresses->Next;
        }
    } else {
        printf("Call to GetAdaptersAddresses failed with error: %d\n",
               dwRetVal);
        if (dwRetVal == ERROR_NO_DATA)
            printf("\tNo addresses were found for the requested parameters\n");
        else {

            if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                    FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 
                    NULL, dwRetVal, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),   
                    // Default language
                    (LPTSTR) & lpMsgBuf, 0, NULL)) {
                printf("\tError: %s", (const char *)lpMsgBuf);
                LocalFree(lpMsgBuf);
                if (pAddresses)
                    FREE(pAddresses);
                return result;
            }
        }
    }

    if (pAddresses) {
        FREE(pAddresses);
    }

    return result;
}
