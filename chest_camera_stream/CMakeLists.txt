cmake_minimum_required(VERSION 3.0)
project(ChestCameraStream)

set(CMAKE_CXX_FLAGS "-std=c++11")

find_package(OpenCV REQUIRED)

include_directories(
    /usr/include/ueye_api/
    /usr/include/hiredis/
    /usr/include/eigen3/
)

link_directories(
    /usr/lib/
    #/usr/lib/x86_64-linux-gnu/
    #        /usr/local/lib
)

set(headers
    cRedis.h
)

set(sources
    cRedis.cpp
)

add_definitions(-DUEYE_API)

#include_directories(
#/usr/include/flycapture/
#)

#include_directories(${PCL_INCLUDE_DIRS})
#link_directories(${PCL_LIBRARY_DIRS})
#add_definitions(${PCL_DEFINITIONS})

#link_directories(
#/usr/lib/
#)


add_executable(
    ${PROJECT_NAME}
    main.cpp
    ${headers}
    ${sources}
)

target_link_libraries(
    ${PROJECT_NAME}
    # Spinnaker
    ueye_api
#    ${OpenCV_LIBS}
    opencv_core
    opencv_imgproc
    opencv_imgcodecs
    opencv_highgui
    hiredis
#    ${PCL_LIBRARIES}
)
