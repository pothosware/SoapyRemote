########################################################################
# Build the remote support library
########################################################################

#build a static library
add_library(SoapySDRRemoteCommon STATIC
    SoapyURLUtils.cpp
    SoapyRPCSocket.cpp
    SoapyRPCPacker.cpp
    SoapyRPCUnpacker.cpp
    SoapyStreamEndpoint.cpp
    SoapyHTTPUtils.cpp
    SoapySSDPEndpoint.cpp
    SoapyIfAddrs.cpp)

target_include_directories(SoapySDRRemoteCommon PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(SoapySDRRemoteCommon PRIVATE SoapySDR)

set_target_properties(SoapySDRRemoteCommon PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

#network interface enumeration
if (WIN32)
    target_sources(SoapySDRRemoteCommon PRIVATE SoapyIfAddrsWindows.cpp)
else ()
    target_sources(SoapySDRRemoteCommon PRIVATE SoapyIfAddrsUnix.cpp)
endif ()

#configure ssdp defines
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/SoapyInfoUtils.in.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/SoapyInfoUtils.cpp
@ONLY)
target_sources(SoapySDRRemoteCommon PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/SoapyInfoUtils.cpp)

#check for platform-specific network header
include(CheckIncludeFiles)
CHECK_INCLUDE_FILES(winsock2.h HAS_WINSOCK2_H)
CHECK_INCLUDE_FILES(ws2tcpip.h HAS_WS2TCPIP_H)
CHECK_INCLUDE_FILES(netdb.h HAS_NETDB_H)
CHECK_INCLUDE_FILES(unistd.h HAS_UNISTD_H)
CHECK_INCLUDE_FILES(netinet/in.h HAS_NETINET_IN_H)
CHECK_INCLUDE_FILES(netinet/tcp.h HAS_NETINET_TCP_H)
CHECK_INCLUDE_FILES(sys/types.h HAS_SYS_TYPES_H)
CHECK_INCLUDE_FILES(sys/socket.h HAS_SYS_SOCKET_H)
CHECK_INCLUDE_FILES(arpa/inet.h HAS_ARPA_INET_H)
CHECK_INCLUDE_FILES(ifaddrs.h HAS_IFADDRS_H)
CHECK_INCLUDE_FILES(net/if.h HAS_NET_IF_H)
CHECK_INCLUDE_FILES(fcntl.h HAS_FCNTL_H)

include(CheckCXXSourceCompiles)
CHECK_CXX_SOURCE_COMPILES("#include <cstring>
int main(void){return strerror_r(0, NULL, 0);}" STRERROR_R_XSI)
if (STRERROR_R_XSI)
    target_compile_definitions(SoapySDRRemoteCommon PRIVATE -DSTRERROR_R_XSI)
endif ()

#network libraries
if (WIN32)
    target_link_libraries(SoapySDRRemoteCommon PRIVATE ws2_32)
endif (WIN32)

#avahi for discovery over mDNS/DNS-SD daemon
if (UNIX AND NOT APPLE)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})
    find_package(Avahi)
    if (NOT AVAHI_FOUND)
        message(WARNING
            "Cannot find Avahi client development files:"
            "Avahi is recommended for device discovery over mDNS."
            "Please install libavahi-client-dev or equivalent.")
    endif ()
endif ()

if (APPLE)
    target_sources(SoapySDRRemoteCommon PRIVATE SoapyMDNSEndpointApple.cpp)
elseif (AVAHI_FOUND)
    message(STATUS "AVAHI_INCLUDE_DIRS=${AVAHI_INCLUDE_DIRS}")
    message(STATUS "AVAHI_LIBRARIES=${AVAHI_LIBRARIES}")
    target_include_directories(SoapySDRRemoteCommon PRIVATE ${AVAHI_INCLUDE_DIRS})
    target_link_libraries(SoapySDRRemoteCommon PRIVATE ${AVAHI_LIBRARIES})
    target_sources(SoapySDRRemoteCommon PRIVATE SoapyMDNSEndpointAvahi.cpp)
else ()
    target_sources(SoapySDRRemoteCommon PRIVATE SoapyMDNSEndpointNone.cpp)
endif ()

#create private include header for network compatibility
target_include_directories(SoapySDRRemoteCommon PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/SoapySocketDefs.in.hpp
    ${CMAKE_CURRENT_BINARY_DIR}/SoapySocketDefs.hpp)

#link threads library
find_package(Threads)
message(STATUS "CMAKE_THREAD_LIBS_INIT: ${CMAKE_THREAD_LIBS_INIT}")
if (CMAKE_THREAD_LIBS_INIT)
    target_link_libraries(SoapySDRRemoteCommon PRIVATE ${CMAKE_THREAD_LIBS_INIT})
endif()
