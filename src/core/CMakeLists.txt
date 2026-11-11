configure_file("${PROJECT_SOURCE_DIR}/cmake/build_identity_data.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/build_identity_data.hpp" @ONLY)
add_library(pgp_core STATIC build_configuration.cpp build_identity.cpp portrait.cpp headquarters.cpp workplace.cpp devices.cpp access.cpp departments.cpp
    counties.cpp social.cpp talent.cpp employee_study.cpp team_dynamics.cpp workplace_mood.cpp people_contacts.cpp contact_finance.cpp employee_departure.cpp people.cpp recruitment.cpp checked_arithmetic.cpp calendar.cpp random.cpp integer_codec.cpp simulation.cpp accounts.cpp company_finance.cpp corporate_tax.cpp capacity.cpp staff_workload.cpp
    allocation.cpp market.cpp market_waitlist.cpp market_open_weights.cpp agent_service.cpp service_monitoring.cpp liquidity.cpp lab.cpp workplace_research.cpp synthetic_data.cpp data_acquisition.cpp business_offers.cpp deployment.cpp deployment_risk.cpp deployment_risk_validation.cpp deployment_rollout.cpp deployment_support.cpp management.cpp pricing.cpp funding_support.cpp governance.cpp governance_validation.cpp public_listing.cpp
    infrastructure.cpp infrastructure_orders.cpp infrastructure_physics.cpp infrastructure_calendar.cpp infrastructure_catalog.cpp infrastructure_validation.cpp
    resilience.cpp resilience_catalog.cpp resilience_incidents.cpp resilience_replicas.cpp resilience_validation.cpp
    civic.cpp civic_calendar.cpp civic_validation.cpp society.cpp authority.cpp frontier_catalog.cpp frontier.cpp frontier_automation.cpp
    succession.cpp succession_validation.cpp personal_life.cpp personal_events.cpp founder_departure.cpp objectives.cpp)
add_library(pgp::core ALIAS pgp_core)
target_include_directories(pgp_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(pgp_core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_features(pgp_core PUBLIC cxx_std_20)
pgp_project_target(pgp_core)
