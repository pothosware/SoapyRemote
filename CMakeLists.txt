########################################################################
# Build Soapy SDR remote support
########################################################################
cmake_minimum_required(VERSION 2.8.7)
project(SoapyRemote CXX C)

#extract changelog version
file(READ "${PROJECT_SOURCE_DIR}/Changelog.txt" changelog_txt)
string(REGEX MATCH "Release ([0-9]+\\.[0-9]+\\.[0-9]+) \\(" CHANGELOG_MATCH "${changelog_txt}")
if(NOT CHANGELOG_MATCH)
    message(FATAL_ERROR "Failed to extract version number from Changelog.txt")
endif(NOT CHANGELOG_MATCH)
set(SOAPY_REMOTE_VERSION "${CMAKE_MATCH_1}")

if (NOT SOAPY_REMOTE_EXTVER)
    include(${PROJECT_SOURCE_DIR}/cmake/Modules/GetGitRevisionDescription.cmake)
    get_git_head_revision(GITREFSPEC GITHASH)
    if (GITHASH)
        string(SUBSTRING "${GITHASH}" 0 8 GITHASH)
        set(SOAPY_REMOTE_EXTVER "g${GITHASH}")
    else (GITHASH)
        set(SOAPY_REMOTE_EXTVER "unknown")
    endif (GITHASH)
endif()

set(SOAPY_REMOTE_VERSION "${SOAPY_REMOTE_VERSION}-${SOAPY_REMOTE_EXTVER}")

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

#link threads in freebsd
if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
    find_package(Threads)
    message(STATUS "CMAKE_THREAD_LIBS_INIT: ${CMAKE_THREAD_LIBS_INIT}")
    list(APPEND SoapySDR_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})
endif()

#enable c++11 extensions for OSX
if (APPLE)
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -Wc++11-extensions")
endif(APPLE)

#common headers used by client and server
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/common)

add_subdirectory(common)
add_subdirectory(client)
add_subdirectory(server)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_subdirectory(system)
endif()

#########################################################################
# summary
#########################################################################
message(STATUS "SoapyRemote version: v${SOAPY_REMOTE_VERSION}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
