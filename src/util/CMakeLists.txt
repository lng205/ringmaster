add_library(util
    conversion.cc
    udp_socket.cc
    socket.cc
    file_descriptor.cc
    address.cc
    timestamp.cc
    timerfd.cc
    serialization.cc
    split.cc
    poller.cc
)

target_include_directories(util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(util PUBLIC video)