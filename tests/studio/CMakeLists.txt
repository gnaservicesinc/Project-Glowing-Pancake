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
add_executable(pgp-canvas-test canvas_test.cpp)
target_link_libraries(pgp-canvas-test PRIVATE pgp::studio_library)
target_include_directories(pgp-canvas-test SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/third_party/json/include")
target_compile_definitions(pgp-canvas-test PRIVATE PGP_CANVAS_FIXTURE="${PROJECT_SOURCE_DIR}/tests/content/fixtures/foundation.json"
    PGP_CANVAS_VERSIONS="${PROJECT_SOURCE_DIR}/project-versions.json" PGP_CANVAS_SCRATCH="${CMAKE_CURRENT_BINARY_DIR}")
pgp_project_target(pgp-canvas-test)
add_test(NAME studio.canvas COMMAND pgp-canvas-test)
add_executable(pgp-world-forms-test world_forms_test.cpp)
target_link_libraries(pgp-world-forms-test PRIVATE pgp::studio_library)
target_compile_definitions(pgp-world-forms-test PRIVATE PGP_WORLD_SOURCE="${PROJECT_SOURCE_DIR}/data/runtime/first-playable.json"
    PGP_WORLD_VERSIONS="${PROJECT_SOURCE_DIR}/project-versions.json" PGP_WORLD_SCRATCH="${CMAKE_CURRENT_BINARY_DIR}")
if(NOT PGP_CORE_ONLY)
    target_sources(pgp-world-forms-test PRIVATE "${PROJECT_SOURCE_DIR}/src/studio/app/world_controller.cpp"
        "${PROJECT_SOURCE_DIR}/src/studio/app/world_controller.hpp")
    target_link_libraries(pgp-world-forms-test PRIVATE Qt6::Core)
    target_include_directories(pgp-world-forms-test PRIVATE "${PROJECT_SOURCE_DIR}/src/studio/app")
    target_compile_definitions(pgp-world-forms-test PRIVATE PGP_WORLD_QT=1)
else()
    target_compile_definitions(pgp-world-forms-test PRIVATE PGP_WORLD_QT=0)
endif()
pgp_project_target(pgp-world-forms-test)
add_test(NAME studio.world-forms COMMAND pgp-world-forms-test)
if(NOT PGP_CORE_ONLY)
    add_executable(pgp-snapshot-controller-test snapshot_controller_test.cpp
        "${PROJECT_SOURCE_DIR}/src/studio/app/snapshot_controller.cpp"
        "${PROJECT_SOURCE_DIR}/src/studio/app/snapshot_controller.hpp")
    target_link_libraries(pgp-snapshot-controller-test PRIVATE pgp::studio_library Qt6::Core)
    target_include_directories(pgp-snapshot-controller-test PRIVATE "${PROJECT_SOURCE_DIR}/src/studio/app")
    target_include_directories(pgp-snapshot-controller-test SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/third_party/json/include")
    target_compile_definitions(pgp-snapshot-controller-test PRIVATE PGP_SNAPSHOT_UI_SOURCE="${PROJECT_SOURCE_DIR}/data/runtime/first-playable.json"
        PGP_SNAPSHOT_UI_VERSIONS="${PROJECT_SOURCE_DIR}/project-versions.json" PGP_SNAPSHOT_UI_SCRATCH="${CMAKE_CURRENT_BINARY_DIR}")
    pgp_project_target(pgp-snapshot-controller-test)
    add_test(NAME studio.graphical-snapshots COMMAND pgp-snapshot-controller-test)
endif()
