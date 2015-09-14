########################################################################
# Build the remote support library
########################################################################
set(COMMON_SOURCES
    SoapyURLUtils.cpp
    #SoapyRPCSocketUDT.cpp
    SoapyRPCSocketTCP.cpp
    SoapyRPCPacker.cpp
    SoapyRPCUnpacker.cpp
)

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

#network libraries
if (WIN32)
    list(APPEND SoapySDR_LIBRARIES ws2_32)
endif (WIN32)

#create private include header for network compatibility
include_directories(${CMAKE_CURRENT_BINARY_DIR})
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/SoapyNetworkDefs.in.hpp
    ${CMAKE_CURRENT_BINARY_DIR}/SoapyNetworkDefs.hpp)

#udt support
get_filename_component(UDT_SRC_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../udt4/src ABSOLUTE)
file(GLOB UDT_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${UDT_SRC_ROOT}/*.cpp)
add_definitions(-DUDT_EXPORTS)
include_directories(${UDT_SRC_ROOT})

#adjust the udt receive buff size for OSX
if(APPLE)
    add_definitions(-DHAS_APPLE_OS)
endif(APPLE)

#build a static library
include_directories(${SoapySDR_INCLUDE_DIRS})
add_library(SoapySDRRemoteCommon STATIC ${COMMON_SOURCES} ${UDT_SOURCES})
target_link_libraries(SoapySDRRemoteCommon ${SoapySDR_LIBRARIES})
set_property(TARGET SoapySDRRemoteCommon PROPERTY POSITION_INDEPENDENT_CODE TRUE)
