// 卡片「组件与组合」：一个"假激光雷达"组件，按固定频率发布大点云
// 运行：ros2 run perf_lab fake_lidar
//   或：ros2 component load /ComponentManager perf_lab perf_lab::FakeLidar
#include <chrono>
#include <cinttypes>
#include <memory>
#include <thread>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_components/register_node_macro.hpp"
#include "sensor_msgs/msg/point_cloud2.hpp"

namespace perf_lab
{

class FakeLidar : public rclcpp::Node
{
public:
  // 组件的构造函数必须接收 NodeOptions：容器靠它传入名字、参数、是否进程内通信等
  explicit FakeLidar(const rclcpp::NodeOptions & options)
  : Node("fake_lidar", options)
  {
    bytes_ = declare_parameter("bytes", 1000000);           // 每帧点云多大
    const double rate_hz = declare_parameter("rate_hz", 10.0);
    by_ref_ = declare_parameter("publish_by_ref", false);  // true：按 const 引用发布
    const double ctor_sleep = declare_parameter("ctor_sleep_s", 0.0);

    if (ctor_sleep > 0.0) {
      // 反例：在构造函数里做耗时操作（比如等硬件）。组件不拥有线程，这会卡住整个容器
      RCLCPP_WARN(get_logger(), "sleeping %.1f s in the constructor", ctor_sleep);
      std::this_thread::sleep_for(std::chrono::duration<double>(ctor_sleep));
    }

    pub_ = create_publisher<sensor_msgs::msg::PointCloud2>("cloud", rclcpp::SensorDataQoS());
    timer_ = create_wall_timer(
      std::chrono::duration<double>(1.0 / rate_hz), [this]() {publish();});
    RCLCPP_INFO(get_logger(), "intra-process %s, %ld bytes per cloud",
      get_node_options().use_intra_process_comms() ? "ON" : "OFF", bytes_);
  }

private:
  void publish()
  {
    auto msg = std::make_unique<sensor_msgs::msg::PointCloud2>();
    msg->header.stamp = now();
    msg->header.frame_id = "laser";
    msg->data.resize(static_cast<size_t>(bytes_));
    msg->width = static_cast<uint32_t>(seq_);   // 借 width 字段当序号
    if (seq_ < 3) {
      RCLCPP_INFO(get_logger(), "pub #%d  msg %p  data %p",
        seq_, static_cast<void *>(msg.get()), static_cast<void *>(msg->data.data()));
    }
    ++seq_;
    if (by_ref_) {
      pub_->publish(*msg);              // 只给引用：rclcpp 必须复制一份
    } else {
      pub_->publish(std::move(msg));    // 交出所有权：进程内可以零拷贝
    }
  }

  int64_t bytes_;
  bool by_ref_;
  int seq_ = 0;
  rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

}  // namespace perf_lab

// 文档：这一行让组件在库被加载时可以被发现，相当于入口
RCLCPP_COMPONENTS_REGISTER_NODE(perf_lab::FakeLidar)
