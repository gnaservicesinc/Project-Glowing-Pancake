qt_add_executable(pgp-editor main.cpp probe_controller.cpp probe_controller.hpp canvas_controller.cpp canvas_controller.hpp
    snapshot_controller.cpp snapshot_controller.hpp world_controller.cpp world_controller.hpp catalog_controller.cpp
    asset_controller.cpp asset_controller.hpp revision_controller.cpp revision_controller.hpp)
target_link_libraries(pgp-editor PRIVATE pgp::bridge pgp::studio_library pgp::graphics pgp::resources
    Qt6::Quick Qt6::QuickControls2)
target_include_directories(pgp-editor SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/third_party/json/include")
pgp_project_target(pgp-editor)
target_compile_definitions(pgp-editor PRIVATE PGP_TEST_HOOKS=$<BOOL:${BUILD_TESTING}>)
qt_add_resources(pgp-editor studio_ui
    PREFIX "/pgp/studio" BASE "${PROJECT_SOURCE_DIR}/qml/studio"
    FILES "${PROJECT_SOURCE_DIR}/qml/studio/Main.qml"
          "${PROJECT_SOURCE_DIR}/qml/studio/FlowForms.qml"
          "${PROJECT_SOURCE_DIR}/qml/studio/FlowInspection.qml"
          "${PROJECT_SOURCE_DIR}/qml/studio/FlowCanvas.qml"
          "${PROJECT_SOURCE_DIR}/qml/studio/CanvasNode.qml"
          "${PROJECT_SOURCE_DIR}/qml/studio/SnapshotReview.qml"
          "${PROJECT_SOURCE_DIR}/qml/studio/RevisionComparison.qml"
          "${PROJECT_SOURCE_DIR}/qml/studio/WorldForms.qml"
          "${PROJECT_SOURCE_DIR}/qml/studio/CatalogActions.qml"
          "${PROJECT_SOURCE_DIR}/qml/studio/AssetEditor.qml"
          "${PROJECT_SOURCE_DIR}/qml/studio/GroupTools.qml")
qt_add_resources(pgp-editor studio_map_ui PREFIX "/pgp/studio" BASE "${PROJECT_SOURCE_DIR}/qml/game"
    FILES "${PROJECT_SOURCE_DIR}/qml/game/DomesticMap.qml")
qt_add_resources(pgp-editor studio_map PREFIX "/pgp/studio" BASE "${PROJECT_SOURCE_DIR}/assets/maps"
    FILES "${PROJECT_SOURCE_DIR}/assets/maps/us-states-2024.json" "${PROJECT_SOURCE_DIR}/assets/maps/CENSUS_TERMS.txt")
qt_add_resources(pgp-editor studio_presentations
    PREFIX "/pgp/studio" BASE "${PROJECT_SOURCE_DIR}/qml/game"
    FILES "${PROJECT_SOURCE_DIR}/qml/game/FlowPresentation.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/WorldButton.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/GameInk.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/DeviceGlyph.qml")

# The opt-in editor component contains only the editor executable. Native Qt
# deployment and source/license bundles are separate qualification work.
install(TARGETS pgp-editor RUNTIME DESTINATION bin COMPONENT ContentStudio EXCLUDE_FROM_ALL)
