add_executable(pgp-build-configuration-test build_configuration_test.cpp)
target_link_libraries(pgp-build-configuration-test PRIVATE pgp::core)
pgp_project_target(pgp-build-configuration-test)
add_test(NAME core.build-configuration COMMAND pgp-build-configuration-test)
foreach(pgp_primitive_test checked_arithmetic state_primitives integer_codec constrained_residue simulation accounts capacity)
    string(REPLACE "_" "-" pgp_primitive_target "${pgp_primitive_test}")
    add_executable(pgp-${pgp_primitive_target}-test ${pgp_primitive_test}_test.cpp)
    target_link_libraries(pgp-${pgp_primitive_target}-test PRIVATE pgp::core)
    pgp_project_target(pgp-${pgp_primitive_target}-test)
    add_test(NAME core.${pgp_primitive_target} COMMAND pgp-${pgp_primitive_target}-test)
endforeach()
add_executable(pgp-primitive-fixture primitive_fixture.cpp)
target_link_libraries(pgp-primitive-fixture PRIVATE pgp::core)
pgp_project_target(pgp-primitive-fixture)
add_test(NAME core.primitive-fixture COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/tools/check_primitive_fixture.py"
    --executable "$<TARGET_FILE:pgp-primitive-fixture>")
add_test(NAME tools.primitive-fixture-checker COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/tools/tests/test_primitive_fixture_checker.py")
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
add_test(NAME headless.foundation-demo COMMAND pgp-headless --foundation-demo)
set_tests_properties(headless.foundation-demo PROPERTIES
    PASS_REGULAR_EXPRESSION "Resume opening choice once; cash: 50000; phase: work_ready")
add_subdirectory(content)
add_subdirectory(runtime)
add_subdirectory(persistence)
add_test(NAME headless.flow-demo COMMAND pgp-headless --flow-demo
    "${PROJECT_SOURCE_DIR}/tests/content/fixtures/foundation.json")
set_tests_properties(headless.flow-demo PROPERTIES
    PASS_REGULAR_EXPRESSION "Matched uninterrupted state on 2014-01-15")
add_test(NAME headless.accounts-demo COMMAND pgp-headless --accounts-demo
    "${PROJECT_SOURCE_DIR}/tests/content/fixtures/foundation.json")
set_tests_properties(headless.accounts-demo PROPERTIES
    PASS_REGULAR_EXPRESSION "Cash: 97200000")
add_test(NAME headless.capacity-demo COMMAND pgp-headless --capacity-demo
    "${PROJECT_SOURCE_DIR}/tests/content/fixtures/foundation.json")
set_tests_properties(headless.capacity-demo PROPERTIES
    PASS_REGULAR_EXPRESSION "training: 60 CU/day, 3600 CU complete")
add_test(NAME headless.validate-content COMMAND pgp-headless --validate-content
    "${PROJECT_SOURCE_DIR}/tests/content/fixtures/foundation.json")
set_tests_properties(headless.validate-content PROPERTIES
    PASS_REGULAR_EXPRESSION "Valid content schema 1; version 0.0.0-dev; definitions 11")
add_test(NAME headless.reject-invalid-content COMMAND pgp-headless --validate-content
    "${PROJECT_SOURCE_DIR}/project-versions.json")
add_test(NAME headless.reject-missing-content COMMAND pgp-headless --validate-content
    "${CMAKE_CURRENT_BINARY_DIR}/absent-content-input.json")
set_tests_properties(headless.reject-invalid-content headless.reject-missing-content PROPERTIES WILL_FAIL TRUE)
set_tests_properties(headless.help PROPERTIES PASS_REGULAR_EXPRESSION "Usage: pgp-headless")
add_test(NAME headless.build-info COMMAND pgp-headless --build-info)
set_tests_properties(headless.build-info PROPERTIES
    PASS_REGULAR_EXPRESSION "Configuration: $<CONFIG>")
add_test(NAME headless.reject-unknown COMMAND pgp-headless --not-a-command)
set_tests_properties(headless.reject-unknown PROPERTIES WILL_FAIL TRUE)
add_test(NAME headless.reject-extra COMMAND pgp-headless --help extra)
set_tests_properties(headless.reject-extra PROPERTIES WILL_FAIL TRUE)

if(NOT PGP_CORE_ONLY)
    add_subdirectory(bridge)
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
