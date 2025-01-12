add_library(app
    encoder.cc
    decoder.cc
    protocol.cc
)

target_include_directories(app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(app PUBLIC util video vpx fec)