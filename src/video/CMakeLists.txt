add_library(video
    sdl.cc
    image.cc
    yuv4mpeg.cc
)

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

target_include_directories(video PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(video PUBLIC util SDL2::SDL2 vpx)
