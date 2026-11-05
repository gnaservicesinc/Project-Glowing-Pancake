qt_add_executable(pgp-editor main.cpp probe_controller.cpp probe_controller.hpp canvas_controller.cpp canvas_controller.hpp)
target_link_libraries(pgp-editor PRIVATE pgp::bridge pgp::studio_library pgp::graphics pgp::resources
    Qt6::Quick Qt6::QuickControls2)
pgp_project_target(pgp-editor)
qt_add_resources(pgp-editor studio_ui
    PREFIX "/pgp/studio" BASE "${PROJECT_SOURCE_DIR}/qml/studio"
    FILES "${PROJECT_SOURCE_DIR}/qml/studio/Main.qml"
          "${PROJECT_SOURCE_DIR}/qml/studio/FlowForms.qml"
          "${PROJECT_SOURCE_DIR}/qml/studio/FlowInspection.qml"
          "${PROJECT_SOURCE_DIR}/qml/studio/FlowCanvas.qml"
          "${PROJECT_SOURCE_DIR}/qml/studio/CanvasNode.qml")
qt_add_resources(pgp-editor studio_presentations
    PREFIX "/pgp/studio" BASE "${PROJECT_SOURCE_DIR}/qml/game"
    FILES "${PROJECT_SOURCE_DIR}/qml/game/FlowPresentation.qml")

# The opt-in editor component contains only the editor executable. Native Qt
# deployment and source/license bundles are separate qualification work.
install(TARGETS pgp-editor RUNTIME DESTINATION bin COMPONENT ContentStudio EXCLUDE_FROM_ALL)
