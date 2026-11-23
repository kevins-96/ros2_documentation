cmake_minimum_required(VERSION 3.20)
project(design_lab)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)  # msg_init_bench 要在优化下测耗时
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(action_msgs REQUIRED)
find_package(rosidl_runtime_cpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(study_interfaces REQUIRED)

set(targets
  graph_mapping_probe
  participant_map
  qos_semantics_lab
  action_result_cache
  msg_init_bench
)
foreach(t ${targets})
  add_executable(${t} src/${t}.cpp)
  target_link_libraries(${t}
    rclcpp::rclcpp
    rclcpp_action::rclcpp_action
    ${action_msgs_TARGETS}
    rosidl_runtime_cpp::rosidl_runtime_cpp
    ${std_msgs_TARGETS}
    ${study_interfaces_TARGETS})
endforeach()

install(TARGETS ${targets} DESTINATION lib/${PROJECT_NAME})
install(PROGRAMS scripts/capture_dds_names.sh DESTINATION lib/${PROJECT_NAME})
ament_package()
