cmake_minimum_required(VERSION 3.15)

project(RtmpMuxer)

message("FFmpegHome," $ENV{FFmpegHome})
message("QTDIR," $ENV{QTDIR})
include_directories($ENV{FFmpegHome}/include)
link_directories($ENV{FFmpegHome}/lib)

include_directories($ENV{QTDIR}/include)
link_directories($ENV{QTDIR}/lib)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
find_package(Qt5 COMPONENTS Widgets  Core REQUIRED)

add_executable(RtmpMuxer main.cpp MainForm.cpp)

if (WIN32)
	target_link_options(${CMAKE_PROJECT_NAME} PUBLIC "$<$<C_COMPILER_ID:MSVC>:-SAFESEH:NO>")
endif()

target_link_libraries(RtmpMuxer   avformat  avcodec Qt5::Widgets Qt5::Core)