add_library(pgp_bridge STATIC campaign_content_recovery.cpp content_recovery_controller.cpp decision_people.cpp department_presentation.cpp access_presentation.cpp recruitment_presentation.cpp device_presentation.cpp workplace_presentation.cpp world_controller.cpp world_controller.hpp founding_controller.cpp founding_controller.hpp reservation_model.cpp reservation_model.hpp
    simulation_worker.cpp simulation_worker.hpp campaign_model.cpp campaign_model.hpp
    campaign_worker.cpp campaign_worker.hpp campaign_controller.cpp campaign_controller.hpp
    management_controller.cpp management_controller.hpp research_presentation.cpp heavyweight_presentation.cpp governance_presentation.cpp heavyweight_presentation.hpp
    infrastructure_controller.cpp infrastructure_controller.hpp resilience_presentation.cpp resilience_presentation.hpp
    civic_controller.cpp civic_controller.hpp
    society_controller.cpp society_controller.hpp
    institutions_controller.cpp institutions_controller.hpp
    frontier_controller.cpp frontier_controller.hpp lab_inputs.hpp
    succession_controller.cpp succession_controller.hpp
    objectives_controller.cpp objectives_controller.hpp
    campaign_snapshot.hpp flow_presentation.cpp flow_presentation.hpp)
add_library(pgp::bridge ALIAS pgp_bridge)
target_sources(pgp_bridge PRIVATE talent_presentation.cpp development_reset.cpp headquarters_presentation.cpp personal_life_presentation.cpp contact_finance_presentation.cpp)
set_target_properties(pgp_bridge PROPERTIES AUTOMOC ON)
target_include_directories(pgp_bridge PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(pgp_bridge PUBLIC pgp::session Qt6::Core)
pgp_project_target(pgp_bridge)

qt_add_resources(pgp_bridge world_baseline
    PREFIX "/pgp/assets" BASE "${PROJECT_SOURCE_DIR}/assets"
    FILES "${PROJECT_SOURCE_DIR}/assets/maps/city-context-2010.json")
