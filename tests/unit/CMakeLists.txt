include(CTest) 
include(Catch)

if (USE_MPM)
    message(STATUS "Building unit test for MPM evaluator")
    add_executable(test_mpmevaluator test_mpmevaluator.C)
    target_link_libraries(test_mpmevaluator PRIVATE Catch2::Catch2WithMain liboofem)

    if (WIN32)
        # 1. Copy the DLL (Only on Windows)
        add_custom_command(
            TARGET test_mpmevaluator 
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:liboofem>
                $<TARGET_FILE_DIR:test_mpmevaluator>
            COMMENT "Copying oofem.dll to test directory to fix Exit code 0xc0000135"
        )
    endif()

    # 2. Debug echo
    add_custom_command(
        TARGET test_mpmevaluator 
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "DEBUG: liboofem directory is: $<TARGET_FILE_DIR:liboofem>"
    )

    # 3. Discover tests
    catch_discover_tests(test_mpmevaluator
        TEST_LIST unit_tests
        PROPERTIES
            ENVIRONMENT "PATH=$<TARGET_FILE_DIR:liboofem>;$ENV{PATH}"
            LABELS "unit"
    )

    message (STATUS "Identified unit tests for ${test_mpmevaluator}: ${unit_tests}")
    # Now apply labels explicitly to all discovered tests 
    set_tests_properties(${unit_tests} PROPERTIES LABELS "unit" )
endif()

# Geometry services of continuum interpolations (evaldNdx, jacobian)
add_executable(test_feinterpol test_feinterpol.C)
target_link_libraries(test_feinterpol PRIVATE Catch2::Catch2WithMain liboofem)
if (WIN32 AND USE_SHARED_LIB)
    # catch_discover_tests runs the executable at build time (POST_BUILD), when oofem.dll is not on PATH;
    # copy it next to the test to avoid exit code 0xc0000135 (DLL not found)
    add_custom_command(
        TARGET test_feinterpol
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:liboofem>
            $<TARGET_FILE_DIR:test_feinterpol>
        COMMENT "Copying oofem.dll to test directory"
    )
endif()
catch_discover_tests(test_feinterpol
    TEST_LIST feinterpol_tests
    PROPERTIES
        ENVIRONMENT "PATH=$<TARGET_FILE_DIR:liboofem>;$ENV{PATH}"
        LABELS "unit"
)
