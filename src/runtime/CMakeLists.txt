add_library(pgp_runtime STATIC compiler.cpp runtime.cpp world.cpp scenario.cpp duration.cpp committed.cpp competition.cpp
    competition_market.cpp competition_planning.cpp competition_validation.cpp wholesale.cpp heavyweights.cpp heavyweight_deals.cpp mergers.cpp heavyweight_validation.cpp society.cpp
    authority.cpp authority_queries.cpp authority_validation.cpp)
add_library(pgp::runtime ALIAS pgp_runtime)
target_include_directories(pgp_runtime PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(pgp_runtime SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/third_party/json/include")
target_link_libraries(pgp_runtime PUBLIC pgp::content)
pgp_project_target(pgp_runtime)
