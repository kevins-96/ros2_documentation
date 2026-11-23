// 第 11 批：ROS 2 的"图接口"（graph interface）长什么样。
//   watch <topic> [秒]  ：等图变化事件（默认看 10 秒），每次变化打印话题的发布者数和端点信息
//   pub <topic> <延时s>  ：延时后创建发布者，打印创建时刻，再过 2 秒退出
// 两个进程的时刻都用 system_clock 毫秒，相减就是"发现 + 图事件"的延迟。
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

using namespace std::chrono_literals;

static long long now_ms()
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
}

static const char * rel(rclcpp::ReliabilityPolicy r)
{
  return r == rclcpp::ReliabilityPolicy::Reliable ? "reliable" :
         r == rclcpp::ReliabilityPolicy::BestEffort ? "best_effort" : "other";
}

static const char * dur(rclcpp::DurabilityPolicy d)
{
  return d == rclcpp::DurabilityPolicy::TransientLocal ? "transient_local" :
         d == rclcpp::DurabilityPolicy::Volatile ? "volatile" : "other";
}

int main(int argc, char ** argv)
{
  auto args = rclcpp::init_and_remove_ros_arguments(argc, argv);
  std::string mode = args.size() > 1 ? args[1] : "watch";
  std::string topic = args.size() > 2 ? args[2] : "/graph_probe";

  if (mode == "pub") {
    double delay = args.size() > 3 ? std::stod(args[3]) : 1.0;
    auto node = std::make_shared<rclcpp::Node>("graph_pub");
    std::this_thread::sleep_for(std::chrono::duration<double>(delay));
    auto pub = node->create_publisher<std_msgs::msg::String>(
      topic, rclcpp::QoS(5).transient_local());
    std::printf("[pub]   t=%lld created publisher on %s\n", now_ms(), topic.c_str());
    std::fflush(stdout);
    std::this_thread::sleep_for(2s);
    std::printf("[pub]   t=%lld exiting\n", now_ms());
    std::fflush(stdout);
    rclcpp::shutdown();
    return 0;
  }

  auto node = std::make_shared<rclcpp::Node>("graph_watch");
  auto event = node->get_graph_event();
  size_t last = node->count_publishers(topic);
  std::printf("[watch] t=%lld start, publishers=%zu\n", now_ms(), last);
  double secs = args.size() > 3 ? std::stod(args[3]) : 10.0;
  auto deadline = std::chrono::steady_clock::now() + std::chrono::duration<double>(secs);
  int events = 0;
  while (rclcpp::ok() && std::chrono::steady_clock::now() < deadline) {
    node->wait_for_graph_change(event, 500ms);
    if (!event->check_and_clear()) {continue;}
    ++events;
    size_t n = node->count_publishers(topic);
    if (n == last) {continue;}  // 图变化很多（参数服务、rosout……），只报我们关心的话题
    std::printf("[watch] t=%lld graph event #%d: publishers on %s = %zu\n",
      now_ms(), events, topic.c_str(), n);
    for (const auto & info : node->get_publishers_info_by_topic(topic)) {
      const auto & q = info.qos_profile();
      std::printf("        node=%s%s%s type=%s reliability=%s durability=%s depth=%zu\n",
        info.node_namespace().c_str(), info.node_namespace() == "/" ? "" : "/",
        info.node_name().c_str(), info.topic_type().c_str(), rel(q.reliability()),
        dur(q.durability()), q.depth());
      const auto & h = info.topic_type_hash();
      std::printf("        type_hash version=%d first bytes=%02x%02x%02x%02x\n",
        h.version, h.value[0], h.value[1], h.value[2], h.value[3]);
    }
    std::fflush(stdout);
    last = n;
  }
  std::printf("[watch] total graph events seen=%d\n", events);
  rclcpp::shutdown();
  return 0;
}
