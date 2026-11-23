// 练习：哪些节点共用一个 DDS 参与者（participant）？
//
// 设计文档：Node to Participant mapping
// 思路：每个节点都会在 /rosout 上建一个发布者。用公开的图 API
// get_publishers_info_by_topic("/rosout") 拿到每个发布者的 GID（16 字节）。
// DDS 实体 GID 的前 12 字节是 RTPS GUID 前缀，同一参与者里的所有实体共享它，
// 所以按前缀分组就能看出"哪几个节点在同一个参与者里"。
//
// 说明：设计文档里的 ros_discovery_info 话题是 rmw 内部用的，rclcpp 订阅不到
// （见卡片"常见坑"），这里改用公开 API。
//
// 用法：ros2 run design_lab participant_map [--wait 秒]
// 适用：ROS 2 Rolling，DDS 类 RMW（rmw_fastrtps_cpp 实测）
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "rclcpp/rclcpp.hpp"

using namespace std::chrono_literals;

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  int wait_s = 2;
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::strcmp(argv[i], "--wait") == 0) {wait_s = std::atoi(argv[i + 1]);}
  }
  auto node = std::make_shared<rclcpp::Node>("participant_map");
  rclcpp::executors::SingleThreadedExecutor exec;
  exec.add_node(node);
  auto t0 = std::chrono::steady_clock::now();
  while (rclcpp::ok() && std::chrono::steady_clock::now() - t0 < std::chrono::seconds(wait_s)) {
    exec.spin_some(50ms);  // 等发现完成
  }

  std::map<std::string, std::vector<std::string>> by_prefix;  // GUID 前缀 -> 节点
  for (const auto & info : node->get_publishers_info_by_topic("/rosout")) {
    const auto & gid = info.endpoint_gid();
    char prefix[40];
    std::snprintf(prefix, sizeof(prefix), "%02x%02x%02x%02x.%02x%02x%02x%02x.%02x%02x%02x%02x",
      gid[0], gid[1], gid[2], gid[3], gid[4], gid[5], gid[6], gid[7],
      gid[8], gid[9], gid[10], gid[11]);
    std::string ns = info.node_namespace();
    by_prefix[prefix].push_back(ns + (ns == "/" ? "" : "/") + info.node_name());
  }
  std::printf("共 %zu 个参与者\n", by_prefix.size());
  for (const auto & [prefix, nodes] : by_prefix) {
    std::printf("  参与者 %s：", prefix.c_str());
    for (const auto & n : nodes) {std::printf(" %s", n.c_str());}
    std::printf("\n");
  }
  rclcpp::shutdown();
  return 0;
}
