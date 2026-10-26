cmake_minimum_required(VERSION 3.16)

project(open_v2g_evcc
    VERSION 0.1.0
    DESCRIPTION "Provisional host/SIL skeleton for a vehicle-side AUTOSAR Classic EVCC"
    LANGUAGES C)

option(OPENV2G_BUILD_TESTS "Build host-only unit and integration tests" ON)
option(OPENV2G_BUILD_EXAMPLES "Build host-only examples" ON)
option(OPENV2G_BUILD_SIMULATION "Build host-only surrounding-component simulators" ON)
option(OPENV2G_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

option(OPENV2G_ENABLE_COVERAGE "Instrument GCC host builds for gcov coverage" OFF)
if(OPENV2G_ENABLE_COVERAGE)
    if(NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR "OPENV2G_ENABLE_COVERAGE currently requires GCC/gcov")
    endif()
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
endif()

function(openv2g_enable_strict_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(OPENV2G_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(OPENV2G_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

add_library(openv2g_core STATIC
    src/common/validation.c
    src/charging_coordinator/charge_state_machine.c
    src/charging_coordinator/permission_manager.c
    src/charging_coordinator/fault_coordinator.c
    src/charging_coordinator/bpt_coordinator.c
    src/charging_coordinator/charging_coordinator.c
    src/iso15118_chrg/iso15118_chrg.c
    src/iso15118_chrg/hlc_event_queue.c
    src/iso15118_chrg/sdp_client.c
    src/iso15118_chrg/tcp_tls_session.c
    src/iso15118_chrg/schema_registry.c
    src/iso15118_chrg/exi_codec.c
    src/iso15118_chrg/sap_negotiator.c
    src/iso15118_chrg/protocol_data_flow.c
    src/iso15118_chrg/protocol_adapter_factory.c
    src/iso15118_chrg/din70121_adapter.c
    src/iso15118_chrg/iso15118_2_adapter.c
    src/iso15118_chrg/subsystem_stubs.c
    src/iso15118_chrg/v2gtp_codec.c
    src/support/cp_pp_manager.c
    src/support/energy_data_provider.c
    src/support/plc_link_manager.c
    src/support/charge_hardware_adapter.c)

target_include_directories(openv2g_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(openv2g_core PUBLIC c_std_11)
openv2g_enable_strict_warnings(openv2g_core)

if(OPENV2G_BUILD_SIMULATION)
    add_subdirectory(simulation)
endif()

if(OPENV2G_BUILD_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()

if(OPENV2G_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# POSIX stubs are isolated from portable core and Windows builds.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND OPENV2G_BUILD_SIMULATION)
    option(OPENV2G_BUILD_LINUX_STUBS "Build Linux OS/RTE/TCP/IP stub environment" ON)
    if(OPENV2G_BUILD_LINUX_STUBS)
        add_subdirectory(simulation/linux)
    endif()
endif()

# Optional host candidate; no third-party source downloads in configure/build.
option(OPENV2G_ENABLE_CBV2G_SAP_POC "Build pinned SAP/DIN/ISO-2 EXI host candidate (legacy option name)" OFF)
set(OPENV2G_CBV2G_SOURCE_DIR "" CACHE PATH "External pinned libcbv2g checkout")
if(OPENV2G_ENABLE_CBV2G_SAP_POC)
    add_subdirectory(integration/protocol/adapters/cbv2g)
endif()
if(TARGET openv2g_linux_bsw AND TARGET openv2g_exi_cbv2g_sap)
    add_subdirectory(simulation/linux/wire)
endif()
if(OPENV2G_BUILD_TESTS AND TARGET openv2g_exi_vector_dump)
    add_subdirectory(tests/oracle)
endif()
