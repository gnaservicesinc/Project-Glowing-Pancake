add_library(pgp_runtime STATIC compiler.cpp runtime.cpp world.cpp scenario.cpp campaign_identity.cpp duration.cpp committed.cpp competition.cpp rival_policy.cpp
    family.cpp family_people.cpp family_plans.cpp family_validation.cpp romance.cpp romance_people.cpp romance_validation.cpp competition_people.cpp industry_people.cpp collaboration.cpp collaboration_validation.cpp competition_market.cpp competition_planning.cpp competition_validation.cpp wholesale.cpp heavyweights.cpp heavyweight_deals.cpp mergers.cpp heavyweight_validation.cpp society.cpp
    authority.cpp authority_queries.cpp authority_validation.cpp legacy.cpp
    objectives.cpp objectives_sources.cpp objective_finance.cpp career.cpp career_transitions.cpp campaign_facts.cpp)
if(NOT PGP_DISTRIBUTION)
    target_sources(pgp_runtime PRIVATE playtest.cpp playtest_setup.cpp)
endif()
if(APPLE)
    target_sources(pgp_runtime PRIVATE rival_policy_metal.mm)
    set_source_files_properties(rival_policy_metal.mm PROPERTIES
        COMPILE_OPTIONS "-fobjc-arc;-std=c++20")
    set_target_properties(pgp_runtime PROPERTIES
        OBJCXX_STANDARD 20
        OBJCXX_STANDARD_REQUIRED ON
        OBJCXX_EXTENSIONS OFF)
    target_compile_definitions(pgp_runtime PRIVATE PGP_HAS_METAL_POLICY=1)
    target_link_libraries(pgp_runtime PUBLIC
        "-framework Foundation"
        "-framework Metal"
        "-framework MetalPerformanceShaders"
        "-framework MetalPerformanceShadersGraph")
endif()
add_library(pgp::runtime ALIAS pgp_runtime)
target_include_directories(pgp_runtime PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(pgp_runtime SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/third_party/json/include")
target_link_libraries(pgp_runtime PUBLIC pgp::content)
target_compile_definitions(pgp_runtime PUBLIC PGP_DISTRIBUTION=$<BOOL:${PGP_DISTRIBUTION}>)
pgp_project_target(pgp_runtime)
