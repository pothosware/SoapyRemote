########################################################################
# Build the remote server application
########################################################################
if (MSVC)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/msvc)
endif ()
include_directories(${SoapySDR_INCLUDE_DIRS})
add_executable(SoapySDRServer SoapyServer.cpp)
target_link_libraries(SoapySDRServer ${SoapySDR_LIBRARIES} SoapySDRRemoteCommon)
install(TARGETS SoapySDRServer DESTINATION bin)
