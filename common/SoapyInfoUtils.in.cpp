// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapySocketDefs.hpp"
#include "SoapyInfoUtils.hpp"

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
    //get host and process ids
    std::string hostname = getHostName();
    int pid = getpid();
    int hid = gethostid();

    //combine for unique process ID
    return "upid://" + hostname + "?pid=" + std::to_string(pid) + "&hid=" + std::to_string(hid);
}

SOAPY_REMOTE_API std::string SoapyInfo::getUserAgent(void)
{
    return "@CMAKE_SYSTEM_NAME@ UPnP/1.1 SoapyRemote/@SoapySDR_VERSION@";
}
