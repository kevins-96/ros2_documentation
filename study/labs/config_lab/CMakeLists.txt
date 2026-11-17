cmake_minimum_required(VERSION 3.20)
project(config_lab)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(lifecycle_msgs REQUIRED)
find_package(std_msgs REQUIRED)

foreach(target
    card_parameter_min    # 卡片「参数」：声明、描述符、三种回调
    param_watcher         # 卡片「参数」：监听另一个节点的参数
    card_lifecycle_min    # 卡片「生命周期节点」
    card_logging_min      # 卡片「日志」
    name_lab)             # 卡片「启动参数与重映射」：名字解析
  add_executable(${target} src/${target}.cpp)
  target_link_libraries(${target}
    rclcpp::rclcpp
    rclcpp_lifecycle::rclcpp_lifecycle
    ${lifecycle_msgs_TARGETS}
    ${std_msgs_TARGETS})
  install(TARGETS ${target} DESTINATION lib/${PROJECT_NAME})
endforeach()

# 文档：launch 文件和配置文件都要装到 share/<包名>/ 下，ros2 launch 才找得到
install(DIRECTORY launch config DESTINATION share/${PROJECT_NAME})

ament_package()
