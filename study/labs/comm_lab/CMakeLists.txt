cmake_minimum_required(VERSION 3.20)
project(comm_lab)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(study_interfaces REQUIRED)

set(targets
  card_topic_min
  card_qos_min
  card_service_min
  card_action_min
  card_interface_min
  qos_lab
  action_lab
)
foreach(t ${targets})
  add_executable(${t} src/${t}.cpp)
  target_link_libraries(${t}
    rclcpp::rclcpp
    rclcpp_action::rclcpp_action
    ${geometry_msgs_TARGETS}
    ${std_msgs_TARGETS}
    ${study_interfaces_TARGETS})
endforeach()

install(TARGETS ${targets} DESTINATION lib/${PROJECT_NAME})
ament_package()
