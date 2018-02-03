########################################################################
# Build Soapy SDR remote support
########################################################################
cmake_minimum_required(VERSION 2.8.7)
project(SoapyRemote CXX C)

# select build type to get optimization flags
if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE "Release")
   message(STATUS "Build type not specified: defaulting to release.")
endif(NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "")

#find soapy sdr
find_package(SoapySDR "0.4.0" NO_MODULE)

#enable c++11 features
if(CMAKE_COMPILER_IS_GNUCXX)

    #C++11 is a required language feature for this project
    include(CheckCXXCompilerFlag)
    CHECK_CXX_COMPILER_FLAG("-std=c++11" HAS_STD_CXX11)
    if(HAS_STD_CXX11)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    else(HAS_STD_CXX11)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
    endif()

    #Thread support enabled (not the same as -lpthread)
    list(APPEND SoapySDR_LIBRARIES -pthread)

endif(CMAKE_COMPILER_IS_GNUCXX)

#enable c++11 extensions for OSX
if (APPLE)
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -Wc++11-extensions")
endif(APPLE)

#common headers used by client and server
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/common)

#avahi for discovery over dnssd
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})
find_package(Avahi)
if (AVAHI_FOUND)
    message(STATUS "AVAHI_INCLUDE_DIRS=${AVAHI_INCLUDE_DIRS}")
    message(STATUS "AVAHI_LIBRARIES=${AVAHI_LIBRARIES}")
endif (AVAHI_FOUND)

add_subdirectory(common)
add_subdirectory(client)
add_subdirectory(server)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_subdirectory(system)
endif()
