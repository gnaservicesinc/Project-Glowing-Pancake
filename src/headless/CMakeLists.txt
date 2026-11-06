add_executable(pgp-headless main.cpp foundation_demo.cpp content_validation.cpp content_file.cpp flow_demo.cpp accounts_demo.cpp market_demo.cpp business_run.cpp lab_demo.cpp events_demo.cpp endurance_run.cpp)
target_link_libraries(pgp-headless PRIVATE pgp::session)
pgp_project_target(pgp-headless)
