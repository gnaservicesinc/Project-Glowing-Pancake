qt_add_executable(pgp-editor main.cpp)
target_link_libraries(pgp-editor PRIVATE pgp::studio_library pgp::graphics pgp::resources
    Qt6::Quick Qt6::QuickControls2)
pgp_project_target(pgp-editor)
qt_add_resources(pgp-editor studio_ui
    PREFIX "/pgp/studio" BASE "${PROJECT_SOURCE_DIR}/qml/studio"
    FILES "${PROJECT_SOURCE_DIR}/qml/studio/Main.qml"
          "${PROJECT_SOURCE_DIR}/qml/studio/FlowForms.qml")

# The opt-in editor component contains only the editor executable. Native Qt
# deployment and source/license bundles are separate qualification work.
install(TARGETS pgp-editor RUNTIME DESTINATION bin COMPONENT ContentStudio EXCLUDE_FROM_ALL)
