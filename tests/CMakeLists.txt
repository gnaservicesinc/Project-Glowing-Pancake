add_executable(pgp-build-configuration-test build_configuration_test.cpp)
target_link_libraries(pgp-build-configuration-test PRIVATE pgp::core)
pgp_project_target(pgp-build-configuration-test)
add_test(NAME core.build-configuration COMMAND pgp-build-configuration-test)
add_executable(pgp-build-identity-test build_identity_test.cpp)
target_link_libraries(pgp-build-identity-test PRIVATE pgp::core)
pgp_project_target(pgp-build-identity-test)
target_compile_definitions(pgp-build-identity-test PRIVATE
    PGP_EXPECTED_GAME_VERSION="${PGP_VERSION_GAME}"
    PGP_EXPECTED_EDITOR_VERSION="${PGP_VERSION_EDITOR}"
    PGP_EXPECTED_CONTENT_VERSION="${PGP_VERSION_CONTENT}"
    PGP_EXPECTED_PHASE="${PGP_VERSION_PHASE}"
    PGP_EXPECTED_MANIFEST_SHA256="${PGP_VERSION_MANIFEST_SHA256}")
add_test(NAME core.build-identity COMMAND pgp-build-identity-test)
add_test(NAME tools.project-versions COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/tools/tests/test_project_versions.py")
add_test(NAME headless.help COMMAND pgp-headless --help)
set_tests_properties(headless.help PROPERTIES PASS_REGULAR_EXPRESSION "Usage: pgp-headless")
add_test(NAME headless.build-info COMMAND pgp-headless --build-info)
set_tests_properties(headless.build-info PROPERTIES
    PASS_REGULAR_EXPRESSION "Configuration: $<CONFIG>")
add_test(NAME headless.reject-unknown COMMAND pgp-headless --not-a-command)
set_tests_properties(headless.reject-unknown PROPERTIES WILL_FAIL TRUE)
add_test(NAME headless.reject-extra COMMAND pgp-headless --help extra)
set_tests_properties(headless.reject-extra PROPERTIES WILL_FAIL TRUE)

if(NOT PGP_CORE_ONLY)
    add_subdirectory(resources)
    add_subdirectory(graphics)
    add_test(NAME probe.build-info COMMAND pgp-probe --build-info)
    set_tests_properties(probe.build-info PROPERTIES
        PASS_REGULAR_EXPRESSION "Phase: ${PGP_VERSION_PHASE}"
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME probe.reject-nonfinite-scale COMMAND pgp-probe --text-scale nan)
    add_test(NAME probe.reject-screenshot-without-smoke COMMAND pgp-probe --screenshot unused.png)
    set_tests_properties(probe.reject-nonfinite-scale probe.reject-screenshot-without-smoke
        PROPERTIES WILL_FAIL TRUE ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    # GUI rendering runs only through an explicitly invoked probe, not CTest.
    # That preserves the build wrapper's no-auto-launch contract.
endif()
