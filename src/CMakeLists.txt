cmake_minimum_required(VERSION 3.22)
project(streamer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(app)
add_subdirectory(fec)
add_subdirectory(util)
add_subdirectory(video)

add_executable(sender app/video_sender.cc)
target_link_libraries(sender app util video)

add_executable(receiver app/video_receiver.cc)
target_link_libraries(receiver app util video)

add_executable(relay app/relay.cc)
target_link_libraries(relay app util fec)

add_executable(test_rlnc_encoding test_rlnc_encoding.cc)
target_link_libraries(test_rlnc_encoding fec)