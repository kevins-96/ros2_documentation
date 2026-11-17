cmake_minimum_required(VERSION 3.20)
project(mw_lab)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rmw REQUIRED)
find_package(std_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)

# 卡片「中间件 RMW 与 DDS」「Domain ID 与发现」「Docker 部署」「安全 SROS2」：
# 打印自己用的 RMW / Domain / 发现配置，按序号统计收发，判断"连没连上、丢没丢"
add_executable(mw_probe src/mw_probe.cpp)
target_link_libraries(mw_probe rclcpp::rclcpp rmw::rmw ${std_msgs_TARGETS})

# 卡片「DDS 调优」：同样多的路径点，不同消息结构的序列化开销
add_executable(serialize_bench src/serialize_bench.cpp)
target_link_libraries(serialize_bench
  rclcpp::rclcpp ${nav_msgs_TARGETS} ${geometry_msgs_TARGETS} ${sensor_msgs_TARGETS})

install(TARGETS mw_probe serialize_bench DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY config docker DESTINATION share/${PROJECT_NAME})

ament_package()
