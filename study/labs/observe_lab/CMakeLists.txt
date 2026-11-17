cmake_minimum_required(VERSION 3.20)
project(observe_lab)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(std_srvs REQUIRED)
find_package(visualization_msgs REQUIRED)
find_package(rosbag2_cpp REQUIRED)

# 卡片「命令行工具」「服务与动作内省」「调试与回溯」「tracing」「RViz」：被观察的假规划器
add_executable(fake_planner src/fake_planner.cpp)
target_link_libraries(fake_planner
  rclcpp::rclcpp
  ${geometry_msgs_TARGETS}
  ${nav_msgs_TARGETS}
  ${std_srvs_TARGETS}
  ${visualization_msgs_TARGETS})

# 卡片「命令行工具」：用代码查询 ROS 图（话题列表、发布者数量和 QoS）
add_executable(graph_probe src/graph_probe.cpp)
target_link_libraries(graph_probe rclcpp::rclcpp)

# 卡片「rosbag 录制回放」：在自己的节点里写 bag、不回放直接读 bag
add_executable(plan_recorder src/plan_recorder.cpp)
target_link_libraries(plan_recorder rclcpp::rclcpp rosbag2_cpp::rosbag2_cpp ${nav_msgs_TARGETS})

add_executable(plan_bag_reader src/plan_bag_reader.cpp)
target_link_libraries(plan_bag_reader rclcpp::rclcpp rosbag2_cpp::rosbag2_cpp ${nav_msgs_TARGETS})

install(TARGETS fake_planner graph_probe plan_recorder plan_bag_reader DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY config DESTINATION share/${PROJECT_NAME})
install(PROGRAMS scripts/callback_durations.py DESTINATION lib/${PROJECT_NAME})

ament_package()
