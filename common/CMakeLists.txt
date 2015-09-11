########################################################################
# Build the remote support library
########################################################################
set(COMMON_SOURCES
    SoapyRPCSocket.cpp
    SoapyRPCPacker.cpp
    SoapyRPCUnpacker.cpp
    SoapySocketSession.cpp
)

#provide 64-bit byteswap for GCC
if(CMAKE_COMPILER_IS_GNUCXX)
    add_definitions(-Dhtonll=__builtin_bswap64)
    add_definitions(-Dntohll=__builtin_bswap64)
endif(CMAKE_COMPILER_IS_GNUCXX)

#check for platform-specific network header
include(CheckIncludeFiles)
CHECK_INCLUDE_FILES(netdb.h HAS_NETDB_H)
if(HAS_NETDB_H)
    add_definitions(-DHAS_NETDB_H)
endif(HAS_NETDB_H)

#udt support
get_filename_component(UDT_SRC_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../udt4/src ABSOLUTE)
file(GLOB UDT_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${UDT_SRC_ROOT}/*.cpp)
add_definitions(-DUDT_EXPORTS)
include_directories(${UDT_SRC_ROOT})

#build a static library
include_directories(${SoapySDR_INCLUDE_DIRS})
add_library(SoapySDRRemoteCommon STATIC ${COMMON_SOURCES} ${UDT_SOURCES})
target_link_libraries(SoapySDRRemoteCommon ${SoapySDR_LIBRARIES})
set_property(TARGET SoapySDRRemoteCommon PROPERTY POSITION_INDEPENDENT_CODE TRUE)
