add_library(app
    encoder.cc
    decoder.cc
    protocol.cc
)

target_include_directories(app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(app PUBLIC util video vpx fec)

# 实验程序
add_executable(exp_a ../../experiments/exp_a_recovery.cc)
target_link_libraries(exp_a fec)

add_executable(exp_f ../../experiments/exp_f_adaptive.cc)
target_link_libraries(exp_f fec)

add_executable(exp_d ../../experiments/exp_d_performance.cc)
target_link_libraries(exp_d fec)

add_executable(exp_c ../../experiments/exp_c_overhead.cc)

add_executable(exp_b ../../experiments/exp_b_comparison.cc)
target_link_libraries(exp_b fec)

add_executable(exp_e ../../experiments/exp_e_latency.cc)
target_link_libraries(exp_e fec)
