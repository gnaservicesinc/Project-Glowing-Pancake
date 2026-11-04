add_executable(pgp-graphics-policy-test graphics_policy_test.cpp)
target_link_libraries(pgp-graphics-policy-test PRIVATE pgp::graphics)
pgp_project_target(pgp-graphics-policy-test)
add_test(NAME graphics.policy COMMAND pgp-graphics-policy-test)
