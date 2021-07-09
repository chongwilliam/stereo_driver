cmake_minimum_required(VERSION 3.0)
project(StreamPtGreyToRedis)

#find_package(OpenCV REQUIRED)


#find_package(PCL 1.3 REQUIRED)


#set(CMAKE_CXX_STANDARD 11)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "-std=c++11")


#add_definitions(-D_GLIBCXX_USE_CXX11_ABI=1)



    include_directories(
        /usr/include/spinnaker/
        /usr/include/hiredis/
        /usr/include/eigen3/
    )

    link_directories(
        /usr/lib/
        #/usr/lib/x86_64-linux-gnu/
        #        /usr/local/lib
    )

    set(headers
        # cCameras.h
        cRedis.h
        PoorMansHelper.h
    )

    set(sources
        # cCameras.cpp
        cRedis.cpp
        PoorMansHelper.cpp
    )

    add_definitions(-DSPINNAKER)



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
    Spinnaker
#    ${OpenCV_LIBS}
    opencv_core
    opencv_imgproc
    opencv_imgcodecs
    opencv_highgui
    hiredis
#    ${PCL_LIBRARIES}
)
