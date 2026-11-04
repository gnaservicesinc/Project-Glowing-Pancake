foreach(pgp_runtime_test compiler runtime)
    add_executable(pgp-${pgp_runtime_test}-test ${pgp_runtime_test}_test.cpp)
    target_link_libraries(pgp-${pgp_runtime_test}-test PRIVATE pgp::runtime)
    target_include_directories(pgp-${pgp_runtime_test}-test SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/third_party/json/include")
    target_compile_definitions(pgp-${pgp_runtime_test}-test PRIVATE
        PGP_RUNTIME_FIXTURE_PATH="${PROJECT_SOURCE_DIR}/tests/content/fixtures/foundation.json")
    pgp_project_target(pgp-${pgp_runtime_test}-test)
    add_test(NAME runtime.${pgp_runtime_test} COMMAND pgp-${pgp_runtime_test}-test)
endforeach()
add_executable(pgp-save-integration-test save_integration_test.cpp)
target_link_libraries(pgp-save-integration-test PRIVATE pgp::persistence)
target_compile_definitions(pgp-save-integration-test PRIVATE
    PGP_RUNTIME_FIXTURE_PATH="${PROJECT_SOURCE_DIR}/tests/content/fixtures/foundation.json"
    PGP_INTEGRATION_SCRATCH_PATH="${CMAKE_CURRENT_BINARY_DIR}")
pgp_project_target(pgp-save-integration-test)
add_test(NAME runtime.save-integration COMMAND pgp-save-integration-test)
set_tests_properties(runtime.save-integration PROPERTIES TIMEOUT 30)
add_executable(pgp-session-policy-test session_policy_test.cpp)
target_link_libraries(pgp-session-policy-test PRIVATE pgp::session)
target_compile_definitions(pgp-session-policy-test PRIVATE
    PGP_RUNTIME_FIXTURE_PATH="${PROJECT_SOURCE_DIR}/tests/content/fixtures/foundation.json"
    PGP_INTEGRATION_SCRATCH_PATH="${CMAKE_CURRENT_BINARY_DIR}")
pgp_project_target(pgp-session-policy-test)
add_test(NAME session.persistence-policy COMMAND pgp-session-policy-test)
set_tests_properties(session.persistence-policy PROPERTIES TIMEOUT 30)
