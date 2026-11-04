add_executable(pgp-headless main.cpp foundation_demo.cpp content_validation.cpp)
target_link_libraries(pgp-headless PRIVATE pgp::content)
pgp_project_target(pgp-headless)
