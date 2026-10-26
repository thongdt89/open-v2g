find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME openv2g_test_exi_reference_vectors
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/differential.py
        --candidate $<TARGET_FILE:openv2g_exi_vector_dump>
        --vectors ${CMAKE_CURRENT_SOURCE_DIR}/independent_vectors.json)
set(OPENV2G_ORACLE_SOURCE_DIR "" CACHE PATH "Optional external OpenV2G checkout for test-only oracle")
if(OPENV2G_ORACLE_SOURCE_DIR)
    find_package(Git REQUIRED)
    execute_process(COMMAND ${GIT_EXECUTABLE} -C "${OPENV2G_ORACLE_SOURCE_DIR}" rev-parse HEAD
        OUTPUT_VARIABLE _oracle_head OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE _status)
    if(NOT _status EQUAL 0 OR NOT _oracle_head STREQUAL "733d4f43cb6579c705d59c7386786ce12a0a0026")
        message(FATAL_ERROR "Independent oracle requires OpenV2G commit 733d4f43cb6579c705d59c7386786ce12a0a0026")
    endif()
    file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/SHA256SUMS _checksums)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/SHA256SUMS)
    foreach(_line IN LISTS _checksums)
        string(SUBSTRING "${_line}" 0 64 _expected)
        string(SUBSTRING "${_line}" 66 -1 _relative)
        set(_file "${OPENV2G_ORACLE_SOURCE_DIR}/${_relative}")
        file(SHA256 "${_file}" _actual)
        if(NOT _actual STREQUAL _expected)
            message(FATAL_ERROR "Oracle source checksum mismatch: ${_relative}")
        endif()
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_file}")
    endforeach()
    # External LGPL code is only compiled into this separate test executable.
    # Keep the external checkout unchanged; configure DIN in a build copy.
    file(COPY "${OPENV2G_ORACLE_SOURCE_DIR}/src" DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/configured)
    file(READ ${CMAKE_CURRENT_BINARY_DIR}/configured/src/din/dinEXIDatatypes.h _din)
    string(REPLACE "#define DEPLOY_DIN_CODEC SUPPORT_NO" "#define DEPLOY_DIN_CODEC SUPPORT_YES" _din "${_din}")
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/configured/src/din/dinEXIDatatypes.h "${_din}")
    set(_sources openv2g_oracle.c)
    set(_includes)
    foreach(_sub codec appHandshake din iso1 xmldsig)
        file(GLOB _sub_sources ${CMAKE_CURRENT_BINARY_DIR}/configured/src/${_sub}/*.c)
        list(APPEND _sources ${_sub_sources})
        list(APPEND _includes ${CMAKE_CURRENT_BINARY_DIR}/configured/src/${_sub})
    endforeach()
    add_executable(openv2g_independent_oracle ${_sources})
    target_include_directories(openv2g_independent_oracle PRIVATE ${_includes})
    target_compile_features(openv2g_independent_oracle PRIVATE c_std_99)
    add_test(NAME openv2g_test_exi_differential
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/differential.py
            --candidate $<TARGET_FILE:openv2g_exi_vector_dump>
            --oracle $<TARGET_FILE:openv2g_independent_oracle>
            --vectors ${CMAKE_CURRENT_SOURCE_DIR}/independent_vectors.json)
    set_tests_properties(openv2g_test_exi_differential PROPERTIES TIMEOUT 30)
endif()

if(TARGET openv2g_exi_feature_vector_dump)
    add_test(NAME openv2g_test_exi_feature_references
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/differential.py
            --candidate $<TARGET_FILE:openv2g_exi_feature_vector_dump>
            --vectors ${CMAKE_CURRENT_SOURCE_DIR}/feature_vectors.json)
    if(TARGET openv2g_independent_oracle)
        add_test(NAME openv2g_test_exi_feature_differential
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/differential.py
                --candidate $<TARGET_FILE:openv2g_exi_feature_vector_dump>
                --oracle $<TARGET_FILE:openv2g_independent_oracle>
                --vectors ${CMAKE_CURRENT_SOURCE_DIR}/feature_vectors.json)
    endif()
endif()
