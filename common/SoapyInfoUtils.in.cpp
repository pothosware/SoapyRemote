// Copyright (c) 2015-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapySocketDefs.hpp"
#include "SoapyInfoUtils.hpp"
#include <cstdlib> //rand
#include <chrono>

#ifdef _MSC_VER

#define getpid() GetCurrentProcessId()

static DWORD gethostid(void)
{
    char szVolName[MAX_PATH];
    char szFileSysName[80];
    DWORD dwSerialNumber;
    DWORD dwMaxComponentLen;
    DWORD dwFileSysFlags;
    GetVolumeInformation(
        "C:\\", szVolName, MAX_PATH,
        &dwSerialNumber, &dwMaxComponentLen,
        &dwFileSysFlags, szFileSysName, sizeof(szFileSysName));
    return dwSerialNumber;
}

#endif //_MSC_VER

std::string SoapyInfo::getHostName(void)
{
    std::string hostname;
    char hostnameBuff[128];
    int ret = gethostname(hostnameBuff, sizeof(hostnameBuff));
    if (ret == 0) hostname = std::string(hostnameBuff);
    else hostname = "unknown";
    return hostname;
}

std::string SoapyInfo::generateUUID1(void)
{
    //64-bit timestamp in nanoseconds
    const auto timeSinceEpoch = std::chrono::high_resolution_clock::now().time_since_epoch();
    const auto timeNanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(timeSinceEpoch);
    const unsigned long long timeNs64 = timeNanoseconds.count();

    //clock sequence (random)
    const unsigned short clockSeq16 = short(std::rand());

    //rather than node, use the host id and pid
    const unsigned short pid16 = short(getpid());
    const unsigned int hid32 = int(gethostid());

    //load up the UUID bytes
    unsigned char bytes[16];
    bytes[0] = (unsigned char)(timeNs64 >> 24);
    bytes[1] = (unsigned char)(timeNs64 >> 16);
    bytes[2] = (unsigned char)(timeNs64 >> 8);
    bytes[3] = (unsigned char)(timeNs64 >> 0);
    bytes[4] = (unsigned char)(timeNs64 >> 40);
    bytes[5] = (unsigned char)(timeNs64 >> 32);
    bytes[6] = (unsigned char)(((timeNs64 >> 56) & 0x0F) | 0x10); //variant
    bytes[7] = (unsigned char)(timeNs64 >> 48);
    bytes[8] = (unsigned char)(((clockSeq16 >> 8) & 0x3F) | 0x80); //reserved
    bytes[9] = (unsigned char)(clockSeq16 >> 0);
    bytes[10] = (unsigned char)(pid16 >> 8);
    bytes[11] = (unsigned char)(pid16 >> 0);
    bytes[12] = (unsigned char)(hid32 >> 24);
    bytes[13] = (unsigned char)(hid32 >> 16);
    bytes[14] = (unsigned char)(hid32 >> 8);
    bytes[15] = (unsigned char)(hid32 >> 0);

    //load fields into the buffer
    char buff[37];
    const int ret = sprintf(buff,
        "%02hhx%02hhx%02hhx%02hhx-%02hhx%02hhx-%02hhx%02hhx-"
        "%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx",
        bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7],
        bytes[8], bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);

    if (ret > 0) return std::string(buff, size_t(ret));
    return ""; //failed
}

SOAPY_REMOTE_API std::string SoapyInfo::getUserAgent(void)
{
    return "@CMAKE_SYSTEM_NAME@ UPnP/1.1 SoapyRemote/@SoapySDR_VERSION@";
}

SOAPY_REMOTE_API std::string SoapyInfo::getServerVersion(void)
{
    return "@SOAPY_REMOTE_VERSION@";
}
