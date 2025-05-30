add_library(app
    encoder.cc
    decoder.cc
    protocol.cc
)

target_include_directories(app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Find FFmpeg components for H265 support
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET
    libavcodec
    libavformat
    libavutil
    libswscale
)

target_link_libraries(app PUBLIC util video PkgConfig::LIBAV fec)