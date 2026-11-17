// 用代码做 "ros2 topic list -t" + "ros2 topic info -v"：节点自己检查 ROS 图
// 典型用途：启动时确认上游话题存在、QoS 兼容，不对就打日志提醒
#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "rclcpp/rclcpp.hpp"

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<rclcpp::Node>("graph_probe");
  const auto topic = node->declare_parameter("topic", std::string("/map"));

  // 发现是异步的：刚启动时图还是空的，等一会儿（真实代码里应等到目标出现或超时）
  std::this_thread::sleep_for(std::chrono::seconds(2));

  for (const auto & [name, types] : node->get_topic_names_and_types()) {
    RCLCPP_INFO(node->get_logger(), "topic %s [%s]", name.c_str(), types.front().c_str());
  }

  RCLCPP_INFO(node->get_logger(), "%s: %zu publishers, %zu subscribers",
    topic.c_str(), node->count_publishers(topic), node->count_subscribers(topic));
  for (const auto & info : node->get_publishers_info_by_topic(topic)) {
    const auto & qos = info.qos_profile();
    RCLCPP_INFO(node->get_logger(), "  publisher %s: reliability=%s durability=%s depth=%zu",
      info.node_name().c_str(),
      qos.reliability() == rclcpp::ReliabilityPolicy::Reliable ? "reliable" : "best_effort",
      qos.durability() == rclcpp::DurabilityPolicy::TransientLocal ? "transient_local" : "volatile",
      qos.depth());
  }
  rclcpp::shutdown();
  return 0;
}
