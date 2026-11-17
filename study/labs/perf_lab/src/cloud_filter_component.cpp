// 卡片「组件与组合」：订阅点云，统计延迟，并打印收到的地址（用来判断有没有复制）
#include <algorithm>
#include <chrono>
#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_components/register_node_macro.hpp"
#include "sensor_msgs/msg/point_cloud2.hpp"

namespace perf_lab
{

using Cloud = sensor_msgs::msg::PointCloud2;

class CloudFilter : public rclcpp::Node
{
public:
  explicit CloudFilter(const rclcpp::NodeOptions & options)
  : Node("cloud_filter", options)
  {
    // "unique"：独占消息，可以就地修改；"shared"：只读共享，多个订阅者也不复制
    const auto mode = declare_parameter("callback", std::string("unique"));
    if (mode == "unique") {
      sub_ = create_subscription<Cloud>(
        "cloud", rclcpp::SensorDataQoS(),
        [this](Cloud::UniquePtr msg) {on_cloud(*msg);});
    } else {
      sub_ = create_subscription<Cloud>(
        "cloud", rclcpp::SensorDataQoS(),
        [this](Cloud::ConstSharedPtr msg) {on_cloud(*msg);});
    }
    report_ = create_wall_timer(std::chrono::seconds(2), [this]() {report();});
  }

private:
  void on_cloud(const Cloud & msg)
  {
    const double lat_ms = (now() - rclcpp::Time(msg.header.stamp)).seconds() * 1e3;
    sum_ms_ += lat_ms;
    max_ms_ = std::max(max_ms_, lat_ms);
    ++n_;
    if (msg.width < 3) {
      RCLCPP_INFO(get_logger(), "sub #%u  msg %p  data %p",
        msg.width, static_cast<const void *>(&msg), static_cast<const void *>(msg.data.data()));
    }
  }

  void report()
  {
    if (n_ == 0) {
      RCLCPP_INFO(get_logger(), "no cloud received");
      return;
    }
    RCLCPP_INFO(get_logger(), "received %d, latency mean %.3f ms, max %.3f ms",
      n_, sum_ms_ / n_, max_ms_);
    n_ = 0;
    sum_ms_ = max_ms_ = 0.0;
  }

  rclcpp::Subscription<Cloud>::SharedPtr sub_;
  rclcpp::TimerBase::SharedPtr report_;
  int n_ = 0;
  double sum_ms_ = 0.0, max_ms_ = 0.0;
};

}  // namespace perf_lab

RCLCPP_COMPONENTS_REGISTER_NODE(perf_lab::CloudFilter)
