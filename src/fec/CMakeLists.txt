add_library(fec
    galois.cc
    rlnc.cc
    intra_frame.cc
)

target_include_directories(fec PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
