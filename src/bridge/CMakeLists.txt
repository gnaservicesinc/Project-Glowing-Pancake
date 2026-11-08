add_library(pgp_bridge STATIC world_controller.cpp world_controller.hpp founding_controller.cpp founding_controller.hpp reservation_model.cpp reservation_model.hpp
    simulation_worker.cpp simulation_worker.hpp campaign_model.cpp campaign_model.hpp
    campaign_worker.cpp campaign_worker.hpp campaign_controller.cpp campaign_controller.hpp
    management_controller.cpp management_controller.hpp heavyweight_presentation.cpp governance_presentation.cpp heavyweight_presentation.hpp
    infrastructure_controller.cpp infrastructure_controller.hpp resilience_presentation.cpp resilience_presentation.hpp
    civic_controller.cpp civic_controller.hpp
    society_controller.cpp society_controller.hpp
    institutions_controller.cpp institutions_controller.hpp
    frontier_controller.cpp frontier_controller.hpp lab_inputs.hpp
    succession_controller.cpp succession_controller.hpp
    objectives_controller.cpp objectives_controller.hpp
    campaign_snapshot.hpp flow_presentation.cpp flow_presentation.hpp)
add_library(pgp::bridge ALIAS pgp_bridge)
set_target_properties(pgp_bridge PROPERTIES AUTOMOC ON)
target_include_directories(pgp_bridge PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(pgp_bridge PUBLIC pgp::session Qt6::Core)
pgp_project_target(pgp_bridge)
