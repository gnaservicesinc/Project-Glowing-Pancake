add_executable(pgp-headless main.cpp)
target_link_libraries(pgp-headless PRIVATE pgp::core)
pgp_project_target(pgp-headless)
