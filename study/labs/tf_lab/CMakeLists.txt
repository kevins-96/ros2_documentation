cmake_minimum_required(VERSION 3.20)
project(tf_lab)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)
find_package(message_filters REQUIRED)

foreach(target
    card_tf2_min          # 卡片「tf2」：静态 + 动态广播
    card_tf2_listener_min # 卡片「tf2」：监听 + transform()
    card_tf2_filter_min   # 卡片「tf2 与时间」：MessageFilter
    card_quaternion_min   # 卡片「四元数」
    tf_time_lab           # 练习：lookupTransform 的时间参数
    tf_offline_lab)       # 练习：四元数与 BufferCore（不需要 ROS 通信）
  add_executable(${target} src/${target}.cpp)
  target_link_libraries(${target}
    rclcpp::rclcpp
    ${geometry_msgs_TARGETS}
    ${sensor_msgs_TARGETS}
    tf2::tf2
    tf2_ros::tf2_ros
    tf2_geometry_msgs::tf2_geometry_msgs
    message_filters::message_filters)
  install(TARGETS ${target} DESTINATION lib/${PROJECT_NAME})
endforeach()

install(DIRECTORY urdf DESTINATION share/${PROJECT_NAME})

ament_package()
