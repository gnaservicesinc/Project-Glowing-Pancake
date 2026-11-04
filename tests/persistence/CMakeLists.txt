add_executable(pgp-persistence-test persistence_test.cpp)
target_link_libraries(pgp-persistence-test PRIVATE pgp::persistence)
target_include_directories(pgp-persistence-test SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/third_party/json/include")
target_compile_definitions(pgp-persistence-test PRIVATE
    PGP_CONTENT_FIXTURE_PATH="${PROJECT_SOURCE_DIR}/tests/content/fixtures/foundation.json"
    PGP_PERSISTENCE_TEST_DIRECTORY="${CMAKE_CURRENT_BINARY_DIR}/scratch")
pgp_project_target(pgp-persistence-test)
add_test(NAME persistence.local-save-settings COMMAND pgp-persistence-test)
set_tests_properties(persistence.local-save-settings PROPERTIES TIMEOUT 30)
