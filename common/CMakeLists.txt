########################################################################
# Build the remote support library
########################################################################
set(COMMON_SOURCES
    SoapyRPCSocket.cpp
    SoapySocketSession.cpp
)

#udt support
get_filename_component(UDT_SRC_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../udt4/src ABSOLUTE)
file(GLOB UDT_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${UDT_SRC_ROOT}/*.cpp)
add_definitions(-DUDT_EXPORTS)
include_directories(${UDT_SRC_ROOT})

#build a static library
include_directories(${SoapySDR_INCLUDE_DIRS})
add_library(SoapySDRRemoteCommon STATIC ${COMMON_SOURCES} ${UDT_SOURCES})
target_link_libraries(SoapySDRRemoteCommon ${SoapySDR_LIBRARIES})
