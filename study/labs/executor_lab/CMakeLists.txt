cmake_minimum_required(VERSION 3.20)
project(executor_lab)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_srvs REQUIRED)

# 实验 A：控制定时器被慢服务阻塞
add_executable(blocking_lab src/blocking_lab.cpp)
target_link_libraries(blocking_lab rclcpp::rclcpp ${std_srvs_TARGETS})

# 实验 B：回调里同步等服务结果导致死锁
add_executable(sync_call_lab src/sync_call_lab.cpp)
target_link_libraries(sync_call_lab rclcpp::rclcpp ${std_srvs_TARGETS})

# 卡片里的最小示例（保证卡片代码能编译）
add_executable(card_executor_min src/card_executor_min.cpp)
target_link_libraries(card_executor_min rclcpp::rclcpp)

add_executable(card_callback_group_min src/card_callback_group_min.cpp)
target_link_libraries(card_callback_group_min rclcpp::rclcpp ${std_srvs_TARGETS})

install(TARGETS blocking_lab sync_call_lab card_executor_min card_callback_group_min
  DESTINATION lib/${PROJECT_NAME})

ament_package()
