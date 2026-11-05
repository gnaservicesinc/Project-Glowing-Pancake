add_library(pgp_bridge STATIC reservation_model.cpp reservation_model.hpp
    simulation_worker.cpp simulation_worker.hpp campaign_model.cpp campaign_model.hpp
    campaign_worker.cpp campaign_worker.hpp campaign_controller.cpp campaign_controller.hpp
    management_controller.cpp management_controller.hpp heavyweight_presentation.cpp heavyweight_presentation.hpp
    campaign_snapshot.hpp flow_presentation.cpp flow_presentation.hpp)
add_library(pgp::bridge ALIAS pgp_bridge)
set_target_properties(pgp_bridge PROPERTIES AUTOMOC ON)
target_include_directories(pgp_bridge PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(pgp_bridge PUBLIC pgp::session Qt6::Core)
pgp_project_target(pgp_bridge)
