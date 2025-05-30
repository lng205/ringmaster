add_library(video
    sdl.cc
    image.cc
    yuv4mpeg.cc
)

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

# Find FFmpeg for image processing
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET
    libavutil
)

target_include_directories(video PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(video PUBLIC util SDL2::SDL2 PkgConfig::LIBAV vpx)
