########################################################################
# Build Soapy SDR remote support
########################################################################
cmake_minimum_required(VERSION 3.1.0)
project(SoapyRemote CXX C)

#C++11 is a required language feature for this project
set(CMAKE_CXX_STANDARD 11)

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

# Use standard GNU install paths
include(GNUInstallDirs)

#find soapy sdr with modern cmake support
find_package(SoapySDR "0.8.0" CONFIG)

if(MSVC)
    #we always want to use multiple cores for compilation
    add_compile_options(/MP)

    #projects should be cross-platform and standard stl functions should work
    add_definitions(-DNOMINMAX) #enables std::min and std::max
endif()

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
