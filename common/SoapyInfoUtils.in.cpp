// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapySocketDefs.hpp"
#include "SoapyInfoUtils.hpp"
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

std::string SoapyInfo::uniqueProcessId(void)
{
    static std::string processUUID;
    if (processUUID.empty())
    {
        char buff[37];

        //65-bit timestamp (lower 60 used)
        const auto timeSinceEpoch = std::chrono::high_resolution_clock::now().time_since_epoch();
        const auto timeNanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(timeSinceEpoch);
        const unsigned long long ticks60 = timeNanoseconds.count();
        const unsigned int timeLow32 = int(ticks60);
        const unsigned int timeMid16 = int(ticks60 >> 32) & 0xffff;
        const unsigned int timeHigh16Ver = (int(ticks60 >> 48) & 0xfff) | (1 << 12);

        //clock sequence (random)
        const int clockSeq16 = std::rand() & 0xffff;

        //rather than node, use the host id and pid
        const unsigned int pid16 = getpid() & 0xffff;
        const unsigned int hid32 = gethostid();

        //load fields into the buffer
        const int ret = sprintf(buff, "%8.8x-%4.4x-%4.4x-%4.4x-%4.4x%8.8x",
            timeLow32, timeMid16, timeHigh16Ver, clockSeq16, pid16, hid32);

        if (ret > 0) processUUID = std::string(buff, size_t(ret));
    }
    return processUUID;
}

SOAPY_REMOTE_API std::string SoapyInfo::getUserAgent(void)
{
    return "@CMAKE_SYSTEM_NAME@ UPnP/1.1 SoapyRemote/@SoapySDR_VERSION@";
}
