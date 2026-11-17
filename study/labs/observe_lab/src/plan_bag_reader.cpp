// 不回放、直接读 bag：统计 /plan 的条数、时间跨度和平均点数
#include <cstdio>
#include <memory>
#include <string>

#include "nav_msgs/msg/path.hpp"
#include "rclcpp/serialization.hpp"
#include "rclcpp/serialized_message.hpp"
#include "rclcpp/time.hpp"
#include "rosbag2_cpp/reader.hpp"

int main(int argc, char ** argv)
{
  if (argc < 2) {
    std::fprintf(stderr, "usage: plan_bag_reader <bag_dir> [topic]\n");
    return 1;
  }
  const std::string topic = argc > 2 ? argv[2] : "/plan";
  rosbag2_cpp::Reader reader;
  reader.open(argv[1]);
  for (const auto & t : reader.get_all_topics_and_types()) {
    std::printf("topic %s [%s]\n", t.name.c_str(), t.type.c_str());
  }

  rclcpp::Serialization<nav_msgs::msg::Path> serialization;
  int count = 0;
  size_t total_points = 0;
  int64_t first_ns = 0, last_ns = 0, first_header_ns = 0;
  while (reader.has_next()) {
    auto bag_msg = reader.read_next();
    if (bag_msg->topic_name != topic) {
      continue;
    }
    rclcpp::SerializedMessage serialized(*bag_msg->serialized_data);
    nav_msgs::msg::Path path;
    serialization.deserialize_message(&serialized, &path);
    // recv_timestamp：写入 bag 时给的时间；header.stamp：消息自己的时间
    if (count == 0) {
      first_ns = bag_msg->recv_timestamp;
      first_header_ns = rclcpp::Time(path.header.stamp).nanoseconds();
    }
    last_ns = bag_msg->recv_timestamp;
    total_points += path.poses.size();
    ++count;
  }
  if (count == 0) {
    std::printf("no messages on %s\n", topic.c_str());
    return 0;
  }
  std::printf("%s: %d messages over %.2f s, %.1f points on average\n", topic.c_str(), count,
    static_cast<double>(last_ns - first_ns) * 1e-9,
    static_cast<double>(total_points) / count);
  std::printf("first message: bag time - header.stamp = %.3f ms\n",
    static_cast<double>(first_ns - first_header_ns) * 1e-6);
  return 0;
}
