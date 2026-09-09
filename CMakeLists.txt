cmake_minimum_required(VERSION 3.16)

project(open_v2g_evcc
    VERSION 0.1.0
    DESCRIPTION "Provisional host/SIL skeleton for a vehicle-side AUTOSAR Classic EVCC"
    LANGUAGES C)

option(OPENV2G_BUILD_TESTS "Build host-only unit and integration tests" ON)
option(OPENV2G_BUILD_EXAMPLES "Build host-only examples" ON)
option(OPENV2G_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

add_library(openv2g_core STATIC
    src/common/validation.c
    src/charging_coordinator/charge_state_machine.c
    src/charging_coordinator/permission_manager.c
    src/charging_coordinator/fault_coordinator.c
    src/charging_coordinator/bpt_coordinator.c
    src/charging_coordinator/charging_coordinator.c
    src/iso15118_chrg/iso15118_chrg.c
    src/iso15118_chrg/sdp_client.c
    src/iso15118_chrg/subsystem_stubs.c
    src/support/cp_pp_manager.c
    src/support/energy_data_provider.c
    src/support/plc_link_manager.c
    src/support/charge_hardware_adapter.c)

target_include_directories(openv2g_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(openv2g_core PUBLIC c_std_11)

if(MSVC)
    target_compile_options(openv2g_core PRIVATE /W4)
    if(OPENV2G_WARNINGS_AS_ERRORS)
        target_compile_options(openv2g_core PRIVATE /WX)
    endif()
else()
    target_compile_options(openv2g_core PRIVATE -Wall -Wextra -Wpedantic)
    if(OPENV2G_WARNINGS_AS_ERRORS)
        target_compile_options(openv2g_core PRIVATE -Werror)
    endif()
endif()

if(OPENV2G_BUILD_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()

if(OPENV2G_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
