add_executable(openv2g_host_demo host_demo/main.c)
target_link_libraries(openv2g_host_demo PRIVATE openv2g_core)
openv2g_enable_strict_warnings(openv2g_host_demo)

if(TARGET openv2g_host_sim)
    add_executable(openv2g_flow_simulator flow_simulator/main.c)
    target_link_libraries(openv2g_flow_simulator PRIVATE openv2g_host_sim)
    openv2g_enable_strict_warnings(openv2g_flow_simulator)
endif()
