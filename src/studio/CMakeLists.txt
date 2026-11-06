add_library(pgp_studio_library STATIC library.cpp draft.cpp flow_forms.cpp trigger_forms.cpp lifecycle_forms.cpp
    execution_forms.cpp effect_forms.cpp variable_forms.cpp flow_probe.cpp snapshot.cpp snapshot_batch.cpp
    layout.cpp flow_graph.cpp graph_edits.cpp graph_trace.cpp world_forms.cpp catalog.cpp reusable_flow.cpp assets.cpp
    revision_review.cpp revision_migration.cpp)
add_library(pgp::studio_library ALIAS pgp_studio_library)
target_include_directories(pgp_studio_library PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(pgp_studio_library SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/third_party/json/include")
target_link_libraries(pgp_studio_library PUBLIC pgp::persistence)
pgp_project_target(pgp_studio_library)

# Explicit maintainer tool, independent of Qt and absent from the game payload.
add_executable(pgp-content-tool snapshot_main.cpp)
target_link_libraries(pgp-content-tool PRIVATE pgp::studio_library)
target_include_directories(pgp-content-tool SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/third_party/json/include")
pgp_project_target(pgp-content-tool)
