add_library(openv2g_host_sim STATIC
    src/host_simulation.c)

target_link_libraries(openv2g_host_sim PUBLIC openv2g_core)
target_include_directories(openv2g_host_sim PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(openv2g_host_sim PUBLIC c_std_11)
openv2g_enable_strict_warnings(openv2g_host_sim)
