// 卡片《接口定义》的最小示例：使用 study_interfaces 里自定义的 PathStatus 消息
// 适用：ROS 2 Rolling（rclcpp）
#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "study_interfaces/msg/path_status.hpp"

using study_interfaces::msg::PathStatus;

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<rclcpp::Node>("path_status_pub");
  auto pub = node->create_publisher<PathStatus>("path_status", 10);

  PathStatus msg;                         // 字段默认值：status=0, length_m=0.0
  msg.header.stamp = node->now();
  msg.header.frame_id = "map";
  msg.status = PathStatus::STATUS_NO_PATH;  // 常量生成为类的静态成员
  // waypoints 是有界数组 [<=1000]，C++ 里生成为 BoundedVector，超过上限 push_back 会抛异常
  msg.waypoints.resize(2);
  try {
    msg.waypoints.resize(1001);
  } catch (const std::exception & e) {
    RCLCPP_WARN(node->get_logger(), "resize(1001) threw: %s", e.what());
  }
  pub->publish(msg);

  RCLCPP_INFO(node->get_logger(), "published status=%u (NO_PATH=%u)",
    msg.status, PathStatus::STATUS_NO_PATH);
  rclcpp::shutdown();
  return 0;
}
