add_executable(pgp-headless main.cpp foundation_demo.cpp content_validation.cpp content_file.cpp flow_demo.cpp accounts_demo.cpp)
target_link_libraries(pgp-headless PRIVATE pgp::persistence)
pgp_project_target(pgp-headless)
