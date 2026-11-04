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
