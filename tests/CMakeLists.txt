add_library(openv2g_test_support STATIC test_support.c)
target_link_libraries(openv2g_test_support PUBLIC openv2g_core)
target_include_directories(openv2g_test_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
openv2g_enable_strict_warnings(openv2g_test_support)

function(add_openv2g_test target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE openv2g_test_support)
    openv2g_enable_strict_warnings(${target})
    add_test(NAME ${target} COMMAND ${target})
endfunction()

function(add_openv2g_simulation_test target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE openv2g_host_sim)
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    openv2g_enable_strict_warnings(${target})
    add_test(NAME ${target} COMMAND ${target})
endfunction()

add_openv2g_test(openv2g_test_permission unit/test_permission_manager.c)
add_openv2g_test(openv2g_test_bpt_coordinator unit/test_bpt_coordinator.c)
add_openv2g_test(openv2g_test_hlc_generation unit/test_hlc_generation.c)
add_openv2g_test(openv2g_test_hlc_event_queue unit/test_hlc_event_queue.c)
add_openv2g_test(openv2g_test_hlc_event_bridge unit/test_hlc_event_bridge.c)
add_openv2g_test(openv2g_test_hlc_tls unit/test_hlc_tls.c)
add_openv2g_test(openv2g_test_sdp_client unit/test_sdp_client.c)
add_openv2g_test(openv2g_test_tcp_tls_session unit/test_tcp_tls_session.c)
add_openv2g_test(openv2g_test_hardware_gate unit/test_hardware_gate.c)
add_openv2g_test(openv2g_test_v2gtp_codec unit/test_v2gtp_codec.c)
add_openv2g_test(openv2g_test_exi_codec unit/test_exi_codec.c)
add_openv2g_test(openv2g_test_sap_negotiator unit/test_sap_negotiator.c)
add_openv2g_test(openv2g_test_protocol_adapters unit/test_protocol_adapters.c)
add_openv2g_test(openv2g_test_protocol_data_flow unit/test_protocol_data_flow.c)
add_openv2g_test(openv2g_test_asr_style_ports unit/test_asr_style_ports.c)
add_openv2g_test(openv2g_test_public_headers unit/test_public_headers.c)
add_openv2g_test(openv2g_test_nominal_flow integration/test_nominal_flow.c)

if(TARGET openv2g_host_sim)
    add_openv2g_simulation_test(
        openv2g_test_host_simulation
        integration/test_host_simulation.c)
endif()
if(TARGET openv2g_host_sim)
    add_openv2g_simulation_test(openv2g_test_processing_budgets integration/test_processing_budgets.c)
endif()

if(TARGET openv2g_host_sim)
    add_openv2g_simulation_test(openv2g_test_control_boundaries integration/test_control_boundaries.c)
endif()

add_openv2g_test(openv2g_test_permission_limits unit/test_permission_limits.c)

add_openv2g_test(openv2g_test_session_context unit/test_session_context.c)

if(TARGET openv2g_host_sim)
    add_openv2g_simulation_test(openv2g_test_communication_budgets integration/test_communication_budgets.c)
endif()

if(TARGET openv2g_host_sim)
    add_openv2g_simulation_test(openv2g_test_certificate_manager unit/test_certificate_manager.c)
endif()

if(TARGET openv2g_host_sim)
    add_openv2g_simulation_test(openv2g_test_pev_slac unit/test_pev_slac.c)
endif()

add_openv2g_test(openv2g_test_pnc_context unit/test_pnc_context.c)

if(TARGET openv2g_host_sim)
    add_openv2g_simulation_test(openv2g_test_coordinator_fault_ingress integration/test_coordinator_fault_ingress.c)
endif()
