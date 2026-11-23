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
find_package(rcl_interfaces REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(lifecycle_msgs REQUIRED)
find_package(rosgraph_msgs REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(rmw_fastrtps_cpp REQUIRED)
find_package(fastdds REQUIRED)

set(targets
  graph_mapping_probe
  participant_map
  qos_semantics_lab
  action_result_cache
  msg_init_bench
  remap_probe
  param_design_probe
  lifecycle_design_probe
  time_design_probe
  rt_alloc_probe
  ipc_design_probe
)
foreach(t ${targets})
  add_executable(${t} src/${t}.cpp)
  target_link_libraries(${t}
    rclcpp::rclcpp
    rclcpp_action::rclcpp_action
    ${action_msgs_TARGETS}
    rosidl_runtime_cpp::rosidl_runtime_cpp
    ${std_msgs_TARGETS}
    ${study_interfaces_TARGETS}
    ${rcl_interfaces_TARGETS}
    rclcpp_lifecycle::rclcpp_lifecycle
    ${lifecycle_msgs_TARGETS}
    ${rosgraph_msgs_TARGETS})
endforeach()

# rt_alloc_probe 自己替换了全局 operator new/delete（malloc/free），GCC 内联后会误报不匹配
set_source_files_properties(src/rt_alloc_probe.cpp PROPERTIES COMPILE_OPTIONS -Wno-mismatched-new-delete)

# 第 10 批：rmw_probe 要用 rmw_fastrtps_cpp 的扩展 API 拿 Fast DDS 原生句柄
add_executable(rmw_probe src/rmw_probe.cpp)
target_link_libraries(rmw_probe
  rclcpp::rclcpp
  ${std_msgs_TARGETS}
  ${geometry_msgs_TARGETS}
  ${study_interfaces_TARGETS}
  rmw_fastrtps_cpp::rmw_fastrtps_cpp
  fastdds)
list(APPEND targets rmw_probe)

# 第 9 批：组件（容器参数会不会传给动态加载的节点）
add_library(name_probe_component SHARED src/name_probe_component.cpp)
target_link_libraries(name_probe_component rclcpp::rclcpp rclcpp_components::component)
rclcpp_components_register_nodes(name_probe_component "design_lab::NameProbe")
install(TARGETS name_probe_component
  ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})

install(TARGETS ${targets} DESTINATION lib/${PROJECT_NAME})
install(PROGRAMS scripts/capture_dds_names.sh scripts/signal_logger.sh DESTINATION lib/${PROJECT_NAME})
ament_package()
