configure_file("${PROJECT_SOURCE_DIR}/cmake/build_identity_data.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/build_identity_data.hpp" @ONLY)
add_library(pgp_core STATIC build_configuration.cpp build_identity.cpp
    checked_arithmetic.cpp calendar.cpp random.cpp integer_codec.cpp simulation.cpp accounts.cpp capacity.cpp
    allocation.cpp market.cpp liquidity.cpp lab.cpp business_offers.cpp deployment.cpp management.cpp governance.cpp governance_validation.cpp
    infrastructure.cpp infrastructure_orders.cpp infrastructure_physics.cpp infrastructure_calendar.cpp infrastructure_catalog.cpp infrastructure_validation.cpp
    resilience.cpp resilience_catalog.cpp resilience_incidents.cpp resilience_replicas.cpp resilience_validation.cpp)
add_library(pgp::core ALIAS pgp_core)
target_include_directories(pgp_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(pgp_core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_features(pgp_core PUBLIC cxx_std_20)
pgp_project_target(pgp_core)
