cmake_minimum_required(VERSION 3.20)
project(path_smoother_plugins)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(path_smoother_core REQUIRED)
find_package(pluginlib REQUIRED)
find_package(rclcpp REQUIRED)

# 插件库必须是 SHARED：pluginlib 在运行时 dlopen 它
add_library(path_smoother_plugins SHARED
  src/moving_average_smoother.cpp
  src/noop_smoother.cpp)
target_link_libraries(path_smoother_plugins
  path_smoother_core::smoother_interface
  pluginlib::pluginlib
  rclcpp::rclcpp)

# 第一个参数是"基类所在的包"，不是本包
pluginlib_export_plugin_description_file(path_smoother_core plugins.xml)

install(TARGETS path_smoother_plugins
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  # 直接链接插件库，不经过 pluginlib，测的是算法本身
  ament_add_gtest(test_moving_average test/test_moving_average.cpp)
  target_link_libraries(test_moving_average path_smoother_plugins)

  # 集成测试：启动 core 的 smoother_node，加载本包插件，检查发布的路径
  find_package(launch_testing_ament_cmake REQUIRED)
  find_package(ament_cmake_ros REQUIRED)
  # run_test_isolated.py：给每个测试分配独立的通信环境，并行测试互不干扰
  add_launch_test(test/test_smoother_node.py
    RUNNER "${ament_cmake_ros_DIR}/run_test_isolated.py"
    TIMEOUT 60)
endif()

ament_package()
