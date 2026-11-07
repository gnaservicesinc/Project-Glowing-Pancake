add_library(pgp_build_options INTERFACE)
add_library(pgp::build_options ALIAS pgp_build_options)
target_compile_features(pgp_build_options INTERFACE cxx_std_20)
target_compile_definitions(pgp_build_options INTERFACE
    PGP_DEVELOPER_DIAGNOSTICS=$<IF:$<CONFIG:Debug>,1,0>
    PGP_BUILD_CONFIGURATION="$<CONFIG>"
)
if(MSVC)
    target_compile_options(pgp_build_options INTERFACE /W4 /permissive- /Zc:__cplusplus)
else()
    target_compile_options(pgp_build_options INTERFACE
        -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
endif()

# Apply to project-owned targets only; never change flags on a dependency.
function(pgp_project_target target)
    target_link_libraries(${target} PRIVATE pgp::build_options)
    set_target_properties(${target} PROPERTIES COMPILE_WARNING_AS_ERROR ON)
endfunction()

# Local development builds may use their host CPU. Distribution/CI defaults
# remain portable unless the build wrapper explicitly selects native tuning.
option(PGP_NATIVE_OPTIMIZATION "Optimize Release for this build machine's CPU" OFF)
if(PGP_NATIVE_OPTIMIZATION AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    include(CheckCXXCompilerFlag)
    if(PGP_TARGET_ARCHITECTURE STREQUAL "arm64")
        set(pgp_native_cpu_flag "-mcpu=native")
    else()
        set(pgp_native_cpu_flag "-march=native")
    endif()
    check_cxx_compiler_flag("${pgp_native_cpu_flag}" PGP_HAS_NATIVE_CPU_FLAG)
    if(NOT PGP_HAS_NATIVE_CPU_FLAG)
        message(FATAL_ERROR "Compiler cannot tune for this CPU; use --portable for this toolchain")
    endif()
    target_compile_options(pgp_build_options INTERFACE "$<$<CONFIG:Release>:${pgp_native_cpu_flag}>")
    message(STATUS "Release CPU optimization: ${pgp_native_cpu_flag}")
endif()
