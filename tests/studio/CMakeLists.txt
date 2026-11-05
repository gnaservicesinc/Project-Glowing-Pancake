add_executable(pgp-studio-library-test library_test.cpp probe_test.cpp)
target_link_libraries(pgp-studio-library-test PRIVATE pgp::studio_library)
target_compile_definitions(pgp-studio-library-test PRIVATE
    PGP_STUDIO_FIXTURE_PATH="${PROJECT_SOURCE_DIR}/tests/content/fixtures/foundation.json"
    PGP_STUDIO_SCRATCH_PATH="${CMAKE_CURRENT_BINARY_DIR}")
pgp_project_target(pgp-studio-library-test)
add_test(NAME studio.library COMMAND pgp-studio-library-test)
set_tests_properties(studio.library PROPERTIES TIMEOUT 30)
add_executable(pgp-snapshot-test snapshot_test.cpp)
target_link_libraries(pgp-snapshot-test PRIVATE pgp::studio_library)
target_include_directories(pgp-snapshot-test SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/third_party/json/include")
target_compile_definitions(pgp-snapshot-test PRIVATE PGP_SNAPSHOT_SOURCE="${PROJECT_SOURCE_DIR}/data/runtime/first-playable.json"
    PGP_VERSION_SOURCE="${PROJECT_SOURCE_DIR}/project-versions.json" PGP_SNAPSHOT_SCRATCH="${CMAKE_CURRENT_BINARY_DIR}")
pgp_project_target(pgp-snapshot-test)
add_test(NAME studio.snapshots COMMAND pgp-snapshot-test)
