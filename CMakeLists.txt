########################################################################
# Build Soapy SDR remote support
########################################################################
cmake_minimum_required(VERSION 2.8.7)
project(SoapyRemote CXX C)

#find soapy sdr
find_package(SoapySDR "0.3.0" NO_MODULE)

#find the pthread library
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

#network libraries
if (WIN32)
    list(APPEND SoapySDR_LIBRARIES ws2_32)
endif (WIN32)

#common headers used by client and server
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/common)

add_subdirectory(common)
add_subdirectory(client)
add_subdirectory(server)
