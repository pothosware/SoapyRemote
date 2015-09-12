########################################################################
# Build Soapy SDR remote support
########################################################################
cmake_minimum_required(VERSION 2.8.7)
project(SoapyRemote CXX C)

#find soapy sdr
find_package(SoapySDR "0.3.0" NO_MODULE)

#enable c++11 features
if(CMAKE_COMPILER_IS_GNUCXX)

    #C++11 is a required language feature for this project
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

endif(CMAKE_COMPILER_IS_GNUCXX)

#find the pthread library (used by udt on unix)
#however udt does not use pthreads on windows
if(UNIX)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_USE_PTHREADS_WIN32 TRUE)
find_package(Threads REQUIRED)
if(NOT THREADS_FOUND)
    message(FATAL_ERROR "pthreads required to compile ${PROJECT_NAME}")
endif()

message (STATUS "THREADS_PTHREADS_INCLUDE_DIR ${THREADS_PTHREADS_INCLUDE_DIR}")
message (STATUS "CMAKE_THREAD_LIBS_INIT ${CMAKE_THREAD_LIBS_INIT}")

list(APPEND SoapySDR_INCLUDE_DIRS ${THREADS_PTHREADS_INCLUDE_DIR})
list(APPEND SoapySDR_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})
endif(UNIX)

#network libraries
if (WIN32)
    list(APPEND SoapySDR_LIBRARIES ws2_32)
endif (WIN32)

#common headers used by client and server
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/common)

add_subdirectory(common)
add_subdirectory(client)
add_subdirectory(server)
