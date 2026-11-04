add_executable(pgp-resource-policy-test resource_guards_test.cpp)
target_link_libraries(pgp-resource-policy-test PRIVATE pgp::resources Qt6::Quick)
pgp_project_target(pgp-resource-policy-test)
qt_add_resources(pgp-resource-policy-test resource-test-fixtures
    PREFIX "/pgp/tests"
    BASE fixtures
    FILES fixtures/allowed.txt fixtures/Local.qml fixtures/checker.png)
add_test(NAME resources.offline-guards COMMAND pgp-resource-policy-test)
set_tests_properties(resources.offline-guards PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QSG_RHI_BACKEND=software"
    TIMEOUT 30)
