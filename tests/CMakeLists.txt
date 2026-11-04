add_executable(pgp-build-configuration-test build_configuration_test.cpp)
target_link_libraries(pgp-build-configuration-test PRIVATE pgp::core)
pgp_project_target(pgp-build-configuration-test)
add_test(NAME core.build-configuration COMMAND pgp-build-configuration-test)
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
    add_test(NAME probe.reject-nonfinite-scale COMMAND pgp-probe --text-scale nan)
    add_test(NAME probe.reject-screenshot-without-smoke COMMAND pgp-probe --screenshot unused.png)
    set_tests_properties(probe.reject-nonfinite-scale probe.reject-screenshot-without-smoke
        PROPERTIES WILL_FAIL TRUE ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    # GUI rendering runs only through an explicitly invoked probe, not CTest.
    # That preserves the build wrapper's no-auto-launch contract.
endif()
