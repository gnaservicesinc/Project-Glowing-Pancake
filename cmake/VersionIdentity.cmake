# One validated reader also serves future packaging tools. Python is build-time only.
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/../authoring/content-state.json"
    "${CMAKE_CURRENT_LIST_DIR}/../authoring/content-history.json"
    "${CMAKE_CURRENT_LIST_DIR}/../data/runtime/content-manifest.json"
    "${CMAKE_CURRENT_LIST_DIR}/../data/runtime/first-playable.json"
    "${CMAKE_CURRENT_LIST_DIR}/../project-versions.json"
    "${CMAKE_CURRENT_LIST_DIR}/../tools/project_versions.py")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/../tools/project_versions.py"
        --manifest "${CMAKE_CURRENT_LIST_DIR}/../project-versions.json" read --format cmake
    RESULT_VARIABLE pgp_version_result OUTPUT_VARIABLE pgp_version_values
    ERROR_VARIABLE pgp_version_error)
if(NOT pgp_version_result EQUAL 0)
    message(FATAL_ERROR "Invalid project version identity: ${pgp_version_error}")
endif()
file(WRITE "${CMAKE_BINARY_DIR}/pgp-version-values.cmake" "${pgp_version_values}")
include("${CMAKE_BINARY_DIR}/pgp-version-values.cmake")
