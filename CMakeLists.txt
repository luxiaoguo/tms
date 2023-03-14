cmake_minimum_required(VERSION 3.10.2)
project(tms)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(CMAKE_BUILD_TYPE DEBUG)
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb -fsanitize=address  -fno-omit-frame-pointer")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

set(INCLUDE_DIR /usr/local/include /usr/include/c++/11/ sdk/ffmpeg/include include sdk/sdp/include /usr/include .)
set(LIB_DIR /usr/local/lib sdk/ffmpeg/lib sdk/sdp/lib)
include_directories(${INCLUDE_DIR})
link_directories(${LIB_DIR})

add_executable(tms
        include/netbase.h
        Utils/pts2dts.hpp
        Utils/pts2dts.cpp
        Utils/Utils.cpp
        Utils/h264SpsParse.c
        source/netbase.cpp 
        source/recver.cpp 
        source/netTcp.cpp 
        source/netUdp.cpp 
        source/mp4Reader.cpp
        source/rtpRecver.cpp
        source/rtpSendPs.cpp
        source/rtspSender.cpp
        source/flvSender.cpp
        source/rtspServer.cpp
        source/define.cpp
        source/rtspParse.cpp
        source/mediaServer.cpp 
        source/tsPacket.cpp 
        source/hlsWriter.cpp 
        source/main.cpp
        )
TARGET_LINK_LIBRARIES(tms asan avformat avcodec avutil sdp crypto ssl pthread z)






