// 卡片《QoS》的最小示例：三种常见导航话题的 QoS 写法，以及"不兼容"事件回调
// 适用：ROS 2 Rolling（rclcpp）
#include <chrono>
#include <memory>

#include "geometry_msgs/msg/twist.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

using namespace std::chrono_literals;

class QosDemo : public rclcpp::Node
{
public:
  QosDemo()
  : Node("qos_demo")
  {
    // 1. 传感器数据：BestEffort + 小队列（rclcpp::SensorDataQoS 预设）
    scan_pub_ = create_publisher<std_msgs::msg::String>("scan_like", rclcpp::SensorDataQoS());

    // 2. 地图这类"只发一次、后来者也要拿到"的数据：Reliable + TransientLocal + depth 1
    auto map_qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local();
    map_pub_ = create_publisher<std_msgs::msg::String>("map_like", map_qos);

    // 3. 控制指令：默认 QoS（KeepLast 10, Reliable, Volatile）
    //    订阅端声明 Deadline，超过 200 ms 没收到就触发回调（可用作"上游掉线"检测）
    rclcpp::SubscriptionOptions opts;
    opts.event_callbacks.deadline_callback =
      [this](rclcpp::QOSDeadlineRequestedInfo & info) {
        RCLCPP_WARN(get_logger(), "cmd_vel deadline missed, total=%d", info.total_count);
      };
    opts.event_callbacks.incompatible_qos_callback =
      [this](rclcpp::QOSRequestedIncompatibleQoSInfo & info) {
        RCLCPP_ERROR(
          get_logger(), "incompatible QoS, last policy kind=%d",
          static_cast<int>(info.last_policy_kind));
      };
    auto cmd_qos = rclcpp::QoS(10).deadline(200ms);
    cmd_sub_ = create_subscription<geometry_msgs::msg::Twist>(
      "cmd_vel", cmd_qos, [](const geometry_msgs::msg::Twist::ConstSharedPtr &) {}, opts);

    std_msgs::msg::String map;
    map.data = "occupancy grid";
    map_pub_->publish(map);  // 只发一次
  }

private:
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr scan_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr map_pub_;
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_sub_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<QosDemo>());
  rclcpp::shutdown();
  return 0;
}
