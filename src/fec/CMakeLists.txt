add_library(fec
    jerasure.cc
)

# Hard-coded path to Jerasure library
target_include_directories(fec PUBLIC /usr/include/jerasure ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(fec PUBLIC /usr/lib/x86_64-linux-gnu/libJerasure.so)