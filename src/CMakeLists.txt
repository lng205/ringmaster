cmake_minimum_required(VERSION 3.28)
project(streamer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(app)
add_subdirectory(util)
add_subdirectory(video)

add_executable(sender app/video_sender.cc)
target_link_libraries(sender app util video)

add_executable(receiver app/video_receiver.cc)
target_link_libraries(receiver app util video)