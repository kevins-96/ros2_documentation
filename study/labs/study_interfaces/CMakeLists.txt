cmake_minimum_required(VERSION 3.20)
project(study_interfaces)

find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(std_msgs REQUIRED)

# 第一个参数必须以包名开头（这里就是包名本身）
rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/Pose2D.msg"
  "msg/PathStatus.msg"
  "msg/FixedGrid.msg"
  "srv/ComputePath.srv"
  "action/MoveTo.action"
  DEPENDENCIES std_msgs
)

ament_export_dependencies(rosidl_default_runtime)
ament_package()
