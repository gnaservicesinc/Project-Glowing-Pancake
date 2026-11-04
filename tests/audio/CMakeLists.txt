add_executable(pgp-audio-test audio_test.cpp)
target_link_libraries(pgp-audio-test PRIVATE pgp::audio)
target_include_directories(pgp-audio-test PRIVATE "${PROJECT_SOURCE_DIR}/src/platform/audio")
target_compile_definitions(pgp-audio-test PRIVATE
    PGP_AUDIO_FIXTURE_ROOT="${PROJECT_SOURCE_DIR}/assets/probe/audio"
    PGP_AUDIO_TEST_SCRATCH_ROOT="${CMAKE_CURRENT_BINARY_DIR}")
pgp_project_target(pgp-audio-test)
add_test(NAME audio.local-playback-contract COMMAND pgp-audio-test)
# Hardware is opened only by an explicit manual pgp-audio-test --native-play.
