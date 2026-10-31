# Build inspected SAP/common/DIN/ISO-2 C sources, with no upstream
# FetchContent, C++ test framework or ISO-20 dependency.
set(_pin 03350be048b35b179905129005a97144a4bdcf93)
set(_manifest "${CMAKE_CURRENT_SOURCE_DIR}/../../generated/libcbv2g/${_pin}/SHA256SUMS")
if(NOT IS_DIRECTORY "${OPENV2G_CBV2G_SOURCE_DIR}")
    message(FATAL_ERROR "Set OPENV2G_CBV2G_SOURCE_DIR to the pinned libcbv2g checkout; see integration/protocol/adapters/cbv2g/README.md")
endif()
find_package(Git REQUIRED)
execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${OPENV2G_CBV2G_SOURCE_DIR}" rev-parse HEAD
    OUTPUT_VARIABLE _head OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE _git_result)
if(NOT _git_result EQUAL 0 OR NOT _head STREQUAL _pin)
    message(FATAL_ERROR "libcbv2g must be checked out at ${_pin}; got ${_head}")
endif()
file(STRINGS "${_manifest}" _checksums)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_manifest}")
foreach(_line IN LISTS _checksums)
    string(SUBSTRING "${_line}" 0 64 _expected)
    string(SUBSTRING "${_line}" 66 -1 _relative)
    set(_file "${OPENV2G_CBV2G_SOURCE_DIR}/${_relative}")
    if(NOT EXISTS "${_file}")
        message(FATAL_ERROR "Missing pinned artifact: ${_relative}")
    endif()
    file(SHA256 "${_file}" _actual)
    if(NOT _actual STREQUAL _expected)
        message(FATAL_ERROR "Pinned artifact checksum mismatch: ${_relative}")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_file}")
endforeach()
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(_cardinality_patch "${CMAKE_CURRENT_SOURCE_DIR}/patches/iso2_transfer_mode_cardinality.py")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_cardinality_patch}")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${_cardinality_patch}"
    "${OPENV2G_CBV2G_SOURCE_DIR}/lib/cbv2g/iso_2" "${CMAKE_CURRENT_BINARY_DIR}/iso2_overlay"
    RESULT_VARIABLE _patch_result)
if(NOT _patch_result EQUAL 0)
    message(FATAL_ERROR "Pinned ISO-2 cardinality overlay validation failed")
endif()
file(SHA256 "${_cardinality_patch}" _patch_digest)
file(SHA256 "${_manifest}" _digest)
string(SHA256 _digest "${_digest};${_patch_digest}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/cbv2g_pin.h"
    "#define OPENV2G_CBV2G_COMMIT \"${_pin}\"\n#define OPENV2G_CBV2G_DIGEST \"${_digest}\"\n")
set(_base "${OPENV2G_CBV2G_SOURCE_DIR}/lib/cbv2g")
add_library(openv2g_cbv2g_sap_generated STATIC
    ${_base}/common/exi_basetypes_decoder.c
    ${_base}/common/exi_basetypes_encoder.c
    ${_base}/common/exi_basetypes.c
    ${_base}/common/exi_bitstream.c
    ${_base}/common/exi_header.c
    ${_base}/common/exi_types_decoder.c
    ${_base}/app_handshake/appHand_Datatypes.c
    ${_base}/app_handshake/appHand_Encoder.c
    ${_base}/app_handshake/appHand_Decoder.c
    ${_base}/din/din_msgDefDatatypes.c
    ${_base}/din/din_msgDefEncoder.c
    ${_base}/din/din_msgDefDecoder.c
    ${_base}/iso_2/iso2_msgDefDatatypes.c
    ${CMAKE_CURRENT_BINARY_DIR}/iso2_overlay/iso2_msgDefEncoder.c
    ${CMAKE_CURRENT_BINARY_DIR}/iso2_overlay/iso2_msgDefDecoder.c)
target_include_directories(openv2g_cbv2g_sap_generated PRIVATE "${OPENV2G_CBV2G_SOURCE_DIR}/include")
target_compile_features(openv2g_cbv2g_sap_generated PRIVATE c_std_11)
# Generated warnings are visible but not promoted to project-owned errors.
if(NOT MSVC)
    target_compile_options(openv2g_cbv2g_sap_generated PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_library(openv2g_exi_cbv2g_sap STATIC exi_cbv2g_sap.c exi_cbv2g_din.c exi_cbv2g_iso2.c exi_cbv2g_dc_defaults.c)
target_include_directories(openv2g_exi_cbv2g_sap PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(openv2g_exi_cbv2g_sap PRIVATE
    "${OPENV2G_CBV2G_SOURCE_DIR}/include" "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(openv2g_exi_cbv2g_sap PUBLIC openv2g_core PRIVATE openv2g_cbv2g_sap_generated)
openv2g_enable_strict_warnings(openv2g_exi_cbv2g_sap)
if(OPENV2G_BUILD_TESTS)
    add_executable(openv2g_test_cbv2g_sap tests/test_cbv2g_sap.c)
    target_link_libraries(openv2g_test_cbv2g_sap PRIVATE openv2g_exi_cbv2g_sap)
    target_include_directories(openv2g_test_cbv2g_sap PRIVATE ${PROJECT_SOURCE_DIR}/tests)
    openv2g_enable_strict_warnings(openv2g_test_cbv2g_sap)
    add_test(NAME openv2g_test_cbv2g_sap COMMAND openv2g_test_cbv2g_sap)
    set_tests_properties(openv2g_test_cbv2g_sap PROPERTIES TIMEOUT 30)
endif()
if(OPENV2G_BUILD_TESTS)
    add_executable(openv2g_test_cbv2g_dc tests/test_cbv2g_dc.c)
    target_link_libraries(openv2g_test_cbv2g_dc PRIVATE openv2g_exi_cbv2g_sap)
    target_include_directories(openv2g_test_cbv2g_dc PRIVATE ${PROJECT_SOURCE_DIR}/tests)
    openv2g_enable_strict_warnings(openv2g_test_cbv2g_dc)
    add_test(NAME openv2g_test_cbv2g_dc COMMAND openv2g_test_cbv2g_dc)
    set_tests_properties(openv2g_test_cbv2g_dc PROPERTIES TIMEOUT 30)
endif()
if(OPENV2G_BUILD_TESTS)
    add_executable(openv2g_exi_vector_dump tests/vector_dump.c)
    target_link_libraries(openv2g_exi_vector_dump PRIVATE openv2g_exi_cbv2g_sap)
    openv2g_enable_strict_warnings(openv2g_exi_vector_dump)
endif()
if(OPENV2G_BUILD_TESTS)
    add_executable(openv2g_test_cbv2g_features tests/test_cbv2g_features.c)
    target_link_libraries(openv2g_test_cbv2g_features PRIVATE openv2g_exi_cbv2g_sap)
    target_include_directories(openv2g_test_cbv2g_features PRIVATE ${PROJECT_SOURCE_DIR}/tests)
    openv2g_enable_strict_warnings(openv2g_test_cbv2g_features)
    add_test(NAME openv2g_test_cbv2g_features COMMAND openv2g_test_cbv2g_features)
endif()
if(OPENV2G_BUILD_TESTS)
    add_executable(openv2g_exi_feature_vector_dump tests/feature_vector_dump.c)
    target_link_libraries(openv2g_exi_feature_vector_dump PRIVATE openv2g_exi_cbv2g_sap)
    openv2g_enable_strict_warnings(openv2g_exi_feature_vector_dump)
endif()

if(OPENV2G_BUILD_TESTS)
    add_executable(openv2g_test_cbv2g_parameters tests/test_cbv2g_parameters.c)
    target_link_libraries(openv2g_test_cbv2g_parameters PRIVATE openv2g_exi_cbv2g_sap)
    target_include_directories(openv2g_test_cbv2g_parameters PRIVATE ${PROJECT_SOURCE_DIR}/tests)
    openv2g_enable_strict_warnings(openv2g_test_cbv2g_parameters)
    add_test(NAME openv2g_test_cbv2g_parameters COMMAND openv2g_test_cbv2g_parameters)
endif()

if(OPENV2G_BUILD_TESTS)
    add_executable(openv2g_test_cbv2g_offers tests/test_cbv2g_offers.c)
    target_link_libraries(openv2g_test_cbv2g_offers PRIVATE openv2g_exi_cbv2g_sap)
    target_include_directories(openv2g_test_cbv2g_offers PRIVATE ${PROJECT_SOURCE_DIR}/tests)
    openv2g_enable_strict_warnings(openv2g_test_cbv2g_offers)
    add_test(NAME openv2g_test_cbv2g_offers COMMAND openv2g_test_cbv2g_offers)
endif()
