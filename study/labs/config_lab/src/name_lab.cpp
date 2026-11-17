// 卡片「启动参数与重映射」：打印每种写法的名字最终解析成什么
// 运行：ros2 run config_lab name_lab
//       ros2 run config_lab name_lab --ros-args -r __ns:=/robot1 -r __node:=front_lidar -r scan:=scan_raw
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<rclcpp::Node>("lidar");
  using S = std_msgs::msg::String;
  auto relative = node->create_publisher<S>("scan", 10);          // 相对名
  auto absolute = node->create_publisher<S>("/diagnostics", 10);  // 绝对名
  auto priv = node->create_publisher<S>("~/status", 10);          // 私有名
  auto nested = node->create_publisher<S>("sensors/scan", 10);    // 相对名里带层级
  std::printf("node      : %s\n", node->get_fully_qualified_name());
  std::printf("scan      -> %s\n", relative->get_topic_name());
  std::printf("/diagnostics -> %s\n", absolute->get_topic_name());
  std::printf("~/status  -> %s\n", priv->get_topic_name());
  std::printf("sensors/scan -> %s\n", nested->get_topic_name());
  std::printf("logger    : %s\n", node->get_logger().get_name());
  std::fflush(stdout);
  // --spin（非 ROS 参数）时保持运行，供 launch 练习使用
  for (const auto & a : rclcpp::remove_ros_arguments(argc, argv)) {
    if (a == "--spin") {rclcpp::spin(node);}
  }
  rclcpp::shutdown();
  return 0;
}
