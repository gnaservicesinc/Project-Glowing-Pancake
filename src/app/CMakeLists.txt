if(BUILD_TESTING)
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

endif()

qt_add_executable(pgp-game game_main.cpp music_controller.cpp music_controller.hpp menu_media.cpp menu_media.hpp)
if(BUILD_TESTING)
    target_sources(pgp-game PRIVATE founding_smoke.cpp founding_smoke.hpp game_smoke.cpp game_smoke.hpp menu_smoke.cpp menu_smoke.hpp)
endif()
target_compile_definitions(pgp-game PRIVATE PGP_TEST_HOOKS=$<BOOL:${BUILD_TESTING}>)
target_link_libraries(pgp-game PRIVATE pgp::bridge pgp::resources pgp::graphics pgp::audio
    Qt6::Quick Qt6::QuickControls2)
pgp_project_target(pgp-game)
qt_add_resources(pgp-game game_ui
    PREFIX "/pgp" BASE "${PROJECT_SOURCE_DIR}/qml"
    FILES "${PROJECT_SOURCE_DIR}/qml/game/Main.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/FoundingFlow.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/MainMenu.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/MenuButton.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/MenuSettings.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/StableEntityList.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/FlowPresentation.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/ManagementPanel.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/AccountsPanel.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/InfrastructurePanel.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/CivicPanel.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/SocietyPanel.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/InstitutionsPanel.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/FrontierPanel.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/SuccessionPanel.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/ObjectivePicker.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/ObjectivesPanel.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/MusicCredits.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/RoomScene.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/DomesticMap.qml")
qt_add_resources(pgp-game menu_assets
    PREFIX "/pgp/assets" BASE "${PROJECT_SOURCE_DIR}/assets"
    FILES "${PROJECT_SOURCE_DIR}/assets/menu/horizon.jpg"
          "${PROJECT_SOURCE_DIR}/assets/menu/icon.png"
          "${PROJECT_SOURCE_DIR}/assets/menu/select.wav")
qt_add_resources(pgp-game founding_images
    PREFIX "/pgp/assets" BASE "${PROJECT_SOURCE_DIR}/assets"
    FILES "${PROJECT_SOURCE_DIR}/assets/founding/california-1997.png"
          "${PROJECT_SOURCE_DIR}/assets/founding/curiosity-2003.png"
          "${PROJECT_SOURCE_DIR}/assets/founding/threshold-2015.png")
if(APPLE)
    set(CMAKE_OBJCXX_COMPILER "${CMAKE_CXX_COMPILER}")
    enable_language(OBJCXX)
    target_sources(pgp-game PRIVATE menu_video_mac.mm
        "${PROJECT_SOURCE_DIR}/assets/menu/Pancake.icns"
        "${PROJECT_SOURCE_DIR}/assets/menu/horizon.mp4")
    set_source_files_properties(menu_video_mac.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    set_source_files_properties("${PROJECT_SOURCE_DIR}/assets/menu/Pancake.icns" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_source_files_properties("${PROJECT_SOURCE_DIR}/assets/menu/horizon.mp4" PROPERTIES MACOSX_PACKAGE_LOCATION Resources/Menu)
    target_link_libraries(pgp-game PRIVATE "-framework AVFoundation" "-framework CoreVideo" "-framework QuartzCore")
    set_target_properties(pgp-game PROPERTIES
        MACOSX_BUNDLE TRUE OUTPUT_NAME "Project Glowing Pancake" OBJCXX_STANDARD 20
        MACOSX_BUNDLE_INFO_PLIST "${PROJECT_SOURCE_DIR}/src/app/Info.plist.in")
endif()
qt_add_resources(pgp-game domestic_map
    PREFIX "/pgp/assets" BASE "${PROJECT_SOURCE_DIR}/assets"
    FILES "${PROJECT_SOURCE_DIR}/assets/maps/us-states-2024.json"
          "${PROJECT_SOURCE_DIR}/assets/maps/CENSUS_TERMS.txt")
set(pgp_music_credits "${PROJECT_SOURCE_DIR}/assets/audio/CREDITS.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${pgp_music_credits}")
file(READ "${pgp_music_credits}" pgp_music_json)
string(JSON pgp_music_count LENGTH "${pgp_music_json}" tracks)
if(pgp_music_count LESS 1 OR pgp_music_count GREATER 16)
    message(FATAL_ERROR "The local soundtrack must have between 1 and 16 reviewed excerpts")
endif()
math(EXPR pgp_music_last "${pgp_music_count} - 1")
set(pgp_music_files "${pgp_music_credits}")
foreach(i RANGE ${pgp_music_last})
    string(JSON track GET "${pgp_music_json}" tracks ${i} path)
    if(NOT track MATCHES "^assets/audio/music/[a-z0-9_]+[.]wav$")
        message(FATAL_ERROR "Only bundled PCM WAV excerpts may enter the soundtrack")
    endif()
    list(APPEND pgp_music_files "${PROJECT_SOURCE_DIR}/${track}")
endforeach()
qt_add_resources(pgp-game soundtrack_files
    PREFIX "/pgp/assets" BASE "${PROJECT_SOURCE_DIR}/assets"
    FILES ${pgp_music_files})
qt_add_resources(pgp-game credits_notices
    PREFIX "/pgp/notices" BASE "${PROJECT_SOURCE_DIR}"
    FILES "${PROJECT_SOURCE_DIR}/LICENSE"
          "${PROJECT_SOURCE_DIR}/assets/maps/CENSUS_TERMS.txt"
          "${PROJECT_SOURCE_DIR}/assets/audio/CC-BY-4.0.txt"
          "${PROJECT_SOURCE_DIR}/assets/notices/Qt-LGPL-3.0.txt"
          "${PROJECT_SOURCE_DIR}/third_party/json/LICENSE.MIT"
          "${PROJECT_SOURCE_DIR}/third_party/audio/miniaudio/LICENSE"
          "${PROJECT_SOURCE_DIR}/third_party/audio/stb/LICENSE")
set(pgp_game_runtime_content "${PROJECT_SOURCE_DIR}/data/runtime/first-playable.json")
if(EXISTS "${PROJECT_SOURCE_DIR}/data/runtime/content-manifest.json")
    list(APPEND pgp_game_runtime_content "${PROJECT_SOURCE_DIR}/data/runtime/content-manifest.json")
endif()
qt_add_resources(pgp-game campaign_content
    PREFIX "/pgp/content" BASE "${PROJECT_SOURCE_DIR}/data/runtime"
    FILES ${pgp_game_runtime_content})
