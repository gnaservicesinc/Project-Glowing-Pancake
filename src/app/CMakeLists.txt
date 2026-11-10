if(BUILD_TESTING)
qt_add_executable(pgp-probe
    main.cpp probe_controller.cpp probe_controller.hpp probe_smoke.cpp probe_smoke.hpp)
target_link_libraries(pgp-probe PRIVATE
    pgp::core pgp::resources pgp::graphics pgp::audio
    Qt6::Quick Qt6::QuickControls2 Qt6::Quick3D)
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

add_library(pgp_portraits STATIC portrait_images.cpp portrait_images.hpp)
target_link_libraries(pgp_portraits PUBLIC pgp::core Qt6::Quick Qt6::Svg)
target_include_directories(pgp_portraits PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
pgp_project_target(pgp_portraits)
qt_add_resources(pgp_portraits portrait_kit PREFIX "/pgp/assets" BASE "${PROJECT_SOURCE_DIR}/assets"
    FILES "${PROJECT_SOURCE_DIR}/assets/portraits/kit-v1.json")
qt_add_executable(pgp-game game_main.cpp music_controller.cpp music_controller.hpp menu_media.cpp menu_media.hpp)
if(BUILD_TESTING)
    find_package(Qt6 6.12.0 EXACT REQUIRED COMPONENTS Test)
    target_sources(pgp-game PRIVATE contact_finance_smoke.cpp hosted_events_smoke.cpp personal_life_smoke.cpp device_interface_smoke.cpp research_smoke.cpp recruitment_smoke.cpp headquarters_smoke.cpp map_layout_smoke.cpp form_drafts_smoke.cpp content_recovery_smoke.cpp world_smoke.cpp world_smoke.hpp founding_smoke.cpp founding_smoke.hpp game_smoke.cpp game_smoke.hpp menu_smoke.cpp menu_smoke.hpp)
    target_link_libraries(pgp-game PRIVATE Qt6::Test)
endif()
target_compile_definitions(pgp-game PRIVATE PGP_TEST_HOOKS=$<BOOL:${BUILD_TESTING}>)
target_link_libraries(pgp-game PRIVATE pgp::bridge pgp::resources pgp::graphics pgp::audio pgp_portraits
    Qt6::Quick Qt6::QuickControls2 Qt6::Quick3D)
pgp_project_target(pgp-game)
qt_add_resources(pgp-game game_ui
    PREFIX "/pgp" BASE "${PROJECT_SOURCE_DIR}/qml"
    FILES "${PROJECT_SOURCE_DIR}/qml/game/Main.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/AppearanceEditor.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/DesktopSurface.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/Workspace3D.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/Headquarters3D.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/HeadquartersPlan.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/EquipmentGlyph.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/EquipmentArt.js"
          "${PROJECT_SOURCE_DIR}/qml/game/HeadquartersTools.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/HeadquartersPanel.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/LegacyWorkplacePanel.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/GameSelect.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/GameCheck.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/GameSlider.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/GameDialog.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/GameField.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/WorldButton.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/WorldDesktop.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/WorldMap.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/MapHoverCard.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/PeoplePanel.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/ContactsPanel.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/ContactFundingPanel.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/PersonalLifePanel.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/FamilyHomePanel.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/BirthKeepsake.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/FamilyRoomScene.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/PersonalEventsPanel.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/RecruitmentDiscussion.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/ManagerPanel.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/WorkplacePanel.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/DevicePanel.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/DeviceFrame.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/DeviceGlyph.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/DeviceHome.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/AccessPanel.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/ReviewPause.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/EmployeePortrait.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/CountyMap.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/StateProjection.js"
          "${PROJECT_SOURCE_DIR}/qml/game/GlobalMap.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/DraftField.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/DraftState.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/DraftComboBox.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/DraftCheckBox.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/FoundingFlow.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/HomeStart.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/HomeOffers.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/FoundingStory.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/GameInk.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/MainMenu.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/MenuButton.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/MenuSettings.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/StableEntityList.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/FlowPresentation.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/DecisionParticipants.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/PersonProfileDialog.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/ManagementPanel.qml"
          "${PROJECT_SOURCE_DIR}/qml/game/ResearchTree.qml"
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
    FILES "${PROJECT_SOURCE_DIR}/assets/founding/hospital.svg"
          "${PROJECT_SOURCE_DIR}/assets/founding/birth-certificate.svg"
          "${PROJECT_SOURCE_DIR}/assets/founding/street.svg"
          "${PROJECT_SOURCE_DIR}/assets/founding/street-apartments.svg"
          "${PROJECT_SOURCE_DIR}/assets/founding/street-hillside.svg"
          "${PROJECT_SOURCE_DIR}/assets/founding/library.svg"
          "${PROJECT_SOURCE_DIR}/assets/founding/workshop.svg"
          "${PROJECT_SOURCE_DIR}/assets/founding/company.svg"
          "${PROJECT_SOURCE_DIR}/assets/founding/trust.svg"
          "${PROJECT_SOURCE_DIR}/assets/ui/laptop.svg"
          "${PROJECT_SOURCE_DIR}/assets/ui/browser.svg"
          "${PROJECT_SOURCE_DIR}/assets/ui/mail.svg")
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
          "${PROJECT_SOURCE_DIR}/assets/maps/CENSUS_TERMS.txt"
          "${PROJECT_SOURCE_DIR}/assets/maps/world-countries.json"
          "${PROJECT_SOURCE_DIR}/assets/maps/NATURAL_EARTH_LICENSE.md")
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

qt_add_resources(pgp-game desktop_art PREFIX "/pgp/assets" BASE "${PROJECT_SOURCE_DIR}/assets" FILES "${PROJECT_SOURCE_DIR}/assets/ui/desktop-wallpaper.svg" "${PROJECT_SOURCE_DIR}/assets/ui/family-bedroom.svg" "${PROJECT_SOURCE_DIR}/assets/ui/hq-oak-floor.svg")
