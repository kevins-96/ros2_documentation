cmake_minimum_required(VERSION 3.20)
project(perf_lab)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)

# 卡片「组件与组合」：两个组件编进同一个共享库
add_library(lidar_components SHARED
  src/fake_lidar_component.cpp
  src/cloud_filter_component.cpp)
target_link_libraries(lidar_components
  rclcpp::rclcpp
  rclcpp_components::component
  ${sensor_msgs_TARGETS})
# 文档：register_node 既注册组件，又生成一个可以单独 ros2 run 的可执行文件
rclcpp_components_register_node(lidar_components
  PLUGIN "perf_lab::FakeLidar"
  EXECUTABLE fake_lidar)
rclcpp_components_register_node(lidar_components
  PLUGIN "perf_lab::CloudFilter"
  EXECUTABLE cloud_filter)
install(TARGETS lidar_components
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)

foreach(target
    ipc_lab      # 卡片「进程内通信与零拷贝」：消息地址变不变
    loan_lab     # 卡片「进程内通信与零拷贝」：借用消息
    rt_lab)      # 卡片「实时编程」：周期抖动、缺页、堆分配
  add_executable(${target} src/${target}.cpp)
  target_link_libraries(${target}
    rclcpp::rclcpp
    ${sensor_msgs_TARGETS}
    ${std_msgs_TARGETS})
  install(TARGETS ${target} DESTINATION lib/${PROJECT_NAME})
endforeach()

install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})

ament_package()
