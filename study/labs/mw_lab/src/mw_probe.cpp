// 连通性探针：一个可执行文件，mode:=pub 发、mode:=sub 收
// 启动时打印"我用的是哪个 RMW、哪个 Domain、什么发现配置、有没有开安全"，
// 订阅端每秒报告：收到多少条、丢了多少条，以及"图里看到几个发布者 / 实际匹配上几个"。
// 后两个数不一样，说明"发现了但没匹配"（QoS 不兼容、分区不同、匹配数上限……）。
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "rmw/rmw.h"
#include "std_msgs/msg/string.hpp"

using namespace std::chrono_literals;

namespace
{
std::string env_or(const char * name, const char * fallback = "(unset)")
{
  const char * v = std::getenv(name);
  return v ? std::string(v) : std::string(fallback);
}
}  // namespace

class MwProbe : public rclcpp::Node
{
public:
  MwProbe()
  : Node("mw_probe")
  {
    mode_ = declare_parameter("mode", std::string("pub"));
    topic_ = declare_parameter("topic", std::string("probe"));
    const double rate_hz = declare_parameter("rate_hz", 5.0);
    text_ = declare_parameter("text", std::string("hello"));
    const auto payload = declare_parameter("payload_bytes", 0);
    const bool reliable = declare_parameter("reliable", true);
    padding_.assign(static_cast<size_t>(payload), 'x');

    // 1. 这个进程到底跑在什么中间件配置上：排查"连不上"时第一件事就是确认两边一致
    const auto domain = get_node_base_interface()->get_context()->get_domain_id();
    RCLCPP_INFO(get_logger(), "rmw=%s domain=%zu", rmw_get_implementation_identifier(), domain);
    RCLCPP_INFO(get_logger(), "discovery: range=%s static_peers=%s discovery_server=%s",
      env_or("ROS_AUTOMATIC_DISCOVERY_RANGE").c_str(), env_or("ROS_STATIC_PEERS").c_str(),
      env_or("ROS_DISCOVERY_SERVER").c_str());
    RCLCPP_INFO(get_logger(), "security: enable=%s strategy=%s keystore=%s",
      env_or("ROS_SECURITY_ENABLE").c_str(), env_or("ROS_SECURITY_STRATEGY").c_str(),
      env_or("ROS_SECURITY_KEYSTORE").c_str());
    RCLCPP_INFO(get_logger(), "fastdds: profiles_file=%s builtin_transports=%s",
      env_or("FASTDDS_DEFAULT_PROFILES_FILE").c_str(),
      env_or("FASTDDS_BUILTIN_TRANSPORTS").c_str());

    rclcpp::QoS qos(10);
    if (!reliable) {
      qos.best_effort();
    }
    const auto period = std::chrono::duration<double>(1.0 / rate_hz);

    if (mode_ == "pub") {
      pub_ = create_publisher<std_msgs::msg::String>(topic_, qos);
      timer_ = create_wall_timer(period, [this]() {
          std_msgs::msg::String msg;
          msg.data = text_ + " #" + std::to_string(seq_++) + " " + padding_;
          pub_->publish(msg);
        });
      report_ = create_wall_timer(1s, [this]() {
          RCLCPP_INFO(get_logger(), "sent #%lu, matched subscriptions=%zu",
            static_cast<unsigned long>(seq_), pub_->get_subscription_count());
        });
    } else {
      // 第二个参数 MessageInfo 带着发布者的 GID，用来按发布者分别数丢包
      sub_ = create_subscription<std_msgs::msg::String>(topic_, qos,
          [this](const std_msgs::msg::String & msg, const rclcpp::MessageInfo & info) {
            on_message(msg, info);
          });
      report_ = create_wall_timer(1s, [this]() {report();});
    }
  }

private:
  void on_message(const std_msgs::msg::String & msg, const rclcpp::MessageInfo & info)
  {
    ++window_;
    ++total_;
    // 从 "hello #123 xxx" 里取出序号；格式不对（比如别人往同一话题发的消息）就只计数、不算丢包
    const auto pos = msg.data.find(" #");
    if (pos == std::string::npos) {
      return;
    }
    const char * begin = msg.data.c_str() + pos + 2;
    char * end = nullptr;
    const long seq = std::strtol(begin, &end, 10);
    if (end == begin) {
      return;
    }
    // 每个发布者各自一条序号流：多个发布者时不能拿 A 的序号去和 B 的比
    const auto & gid = info.get_rmw_message_info().publisher_gid;
    std::array<uint8_t, RMW_GID_STORAGE_SIZE> key{};
    std::copy(std::begin(gid.data), std::end(gid.data), key.begin());
    auto [it, inserted] = last_seq_by_pub_.try_emplace(key, seq);
    if (!inserted) {
      if (seq > it->second + 1) {
        lost_ += seq - it->second - 1;
      }
      it->second = seq;
    }
    last_seq_ = seq;
  }

  void report()
  {
    // 图里的发布者数：来自发现信息；匹配数：DDS 层真正配对成功的
    const auto in_graph = count_publishers(sub_->get_topic_name());
    const auto matched = sub_->get_publisher_count();
    RCLCPP_INFO(get_logger(), "got %ld in last 1 s (total %ld, lost %ld, last #%ld) "
      "publishers in graph=%zu matched=%zu",
      window_, total_, lost_, last_seq_, in_graph, matched);
    window_ = 0;
  }

  std::string mode_, topic_, text_, padding_;
  uint64_t seq_{0};
  long last_seq_{-1}, lost_{0}, window_{0}, total_{0};
  std::map<std::array<uint8_t, RMW_GID_STORAGE_SIZE>, long> last_seq_by_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr pub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr sub_;
  rclcpp::TimerBase::SharedPtr timer_, report_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<MwProbe>());
  rclcpp::shutdown();
  return 0;
}
