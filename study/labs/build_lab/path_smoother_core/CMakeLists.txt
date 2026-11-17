cmake_minimum_required(VERSION 3.20)
project(path_smoother_core)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# 练习「测试」：colcon build --cmake-args -DINJECT_BUG=ON 故意引入一个 bug，看单元测试怎么报
option(INJECT_BUG "Break moving_average on purpose (testing lab)" OFF)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(pluginlib REQUIRED)

# ---------- 1. 纯 C++ 库：不依赖 ROS，方便单元测试 ----------
add_library(path_math src/path_math.cpp)
target_include_directories(path_math PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:include/${PROJECT_NAME}>")
if(INJECT_BUG)
  target_compile_definitions(path_math PRIVATE PATH_SMOOTHER_INJECT_BUG)
endif()

# ---------- 2. 插件基类：只有头文件，但下游要能找到它和它依赖的 rclcpp ----------
add_library(smoother_interface INTERFACE)
target_include_directories(smoother_interface INTERFACE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:include/${PROJECT_NAME}>")
target_link_libraries(smoother_interface INTERFACE path_math rclcpp::rclcpp)

# ---------- 3. 节点：按参数名字加载插件 ----------
add_executable(smoother_node src/smoother_node.cpp)
target_link_libraries(smoother_node
  smoother_interface
  pluginlib::pluginlib
  ${nav_msgs_TARGETS}
  ${geometry_msgs_TARGETS})

# ---------- 安装：头文件、库（带导出集）、可执行文件、配置 ----------
install(DIRECTORY include/ DESTINATION include/${PROJECT_NAME})
install(TARGETS path_math smoother_interface
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)
install(TARGETS smoother_node DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY config launch DESTINATION share/${PROJECT_NAME})

# 下游 find_package(path_smoother_core) 之后就能用 path_smoother_core::path_math
ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)
# 下游也会自动 find_package 这些依赖（基类头文件里用到了 rclcpp）
ament_export_dependencies(rclcpp pluginlib)

if(BUILD_TESTING)
  # 代码规范检查：按 package.xml 里的 test_depend 自动加入 ament_lint_common 的各个 linter
  find_package(ament_lint_auto REQUIRED)
  # 练习文件没有写版权和许可证头，所以跳过这两个检查（ros2 pkg create 生成的模板也是这样写的）
  # 练习「测试」：注释掉这两行，看 colcon test 怎么报"缺版权声明"
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()

  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_path_math test/test_path_math.cpp)
  target_link_libraries(test_path_math path_math)
endif()

ament_package()
