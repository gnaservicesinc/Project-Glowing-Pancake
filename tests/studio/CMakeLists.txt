add_executable(pgp-studio-library-test library_test.cpp probe_test.cpp)
target_link_libraries(pgp-studio-library-test PRIVATE pgp::studio_library)
target_compile_definitions(pgp-studio-library-test PRIVATE
    PGP_STUDIO_FIXTURE_PATH="${PROJECT_SOURCE_DIR}/tests/content/fixtures/foundation.json"
    PGP_STUDIO_SCRATCH_PATH="${CMAKE_CURRENT_BINARY_DIR}")
pgp_project_target(pgp-studio-library-test)
add_test(NAME studio.library COMMAND pgp-studio-library-test)
set_tests_properties(studio.library PROPERTIES TIMEOUT 30)
