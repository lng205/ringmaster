add_library(fec
    jerasure.cc
    intra_frame.cc
)

find_library(JERASURE_LIB Jerasure REQUIRED)
target_include_directories(fec PUBLIC /usr/include/jerasure ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(fec PUBLIC ${JERASURE_LIB})