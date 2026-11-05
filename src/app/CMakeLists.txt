qt_add_executable(pgp-probe
    main.cpp probe_controller.cpp probe_controller.hpp probe_smoke.cpp probe_smoke.hpp)
target_link_libraries(pgp-probe PRIVATE
    pgp::core pgp::resources pgp::graphics pgp::audio
    Qt6::Quick Qt6::QuickControls2)
pgp_project_target(pgp-probe)

qt_add_resources(pgp-probe probe_ui
    PREFIX "/pgp" BASE "${PROJECT_SOURCE_DIR}/qml"
    FILES "${PROJECT_SOURCE_DIR}/qml/probe/Main.qml")
qt_add_resources(pgp-probe probe_images
    PREFIX "/pgp/assets" BASE "${PROJECT_SOURCE_DIR}/assets"
    FILES "${PROJECT_SOURCE_DIR}/assets/probe/images/office.png"
          "${PROJECT_SOURCE_DIR}/assets/probe/images/paper.jpg"
          "${PROJECT_SOURCE_DIR}/assets/probe/audio/tone.wav")

qt_add_executable(pgp-game game_main.cpp game_smoke.cpp game_smoke.hpp)
target_link_libraries(pgp-game PRIVATE pgp::bridge pgp::resources pgp::graphics
    Qt6::Quick Qt6::QuickControls2)
pgp_project_target(pgp-game)
qt_add_resources(pgp-game game_ui
    PREFIX "/pgp" BASE "${PROJECT_SOURCE_DIR}/qml"
    FILES "${PROJECT_SOURCE_DIR}/qml/game/Main.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/StableEntityList.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/FlowPresentation.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/ManagementPanel.qml")
qt_add_resources(pgp-game campaign_content
    PREFIX "/pgp/content" BASE "${PROJECT_SOURCE_DIR}/data/runtime"
    FILES "${PROJECT_SOURCE_DIR}/data/runtime/first-playable.json")
