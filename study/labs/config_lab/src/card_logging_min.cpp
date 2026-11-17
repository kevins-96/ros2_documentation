// 卡片「日志」最小代码：级别、节流、只打一次、子 logger、外部改级别的服务
// 运行：ros2 run config_lab card_logging_min
//       ros2 run config_lab card_logging_min --ros-args --log-level battery_monitor:=debug
#include <chrono>
#include <memory>

#include "rclcpp/rclcpp.hpp"

using namespace std::chrono_literals;

class BatteryMonitor : public rclcpp::Node
{
public:
  BatteryMonitor()
  : Node("battery_monitor", rclcpp::NodeOptions().enable_logger_service(true))
  {
    driver_logger_ = get_logger().get_child("driver");   // 名字是 battery_monitor.driver
    timer_ = create_wall_timer(100ms, [this]() {tick();});
    RCLCPP_INFO(get_logger(), "started, level %d%%", level_);
  }

private:
  void tick()
  {
    ++ticks_;
    if (ticks_ % 5 == 0) {level_ -= 5;}     // 每 0.5 s 掉 5%
    RCLCPP_DEBUG(get_logger(), "tick %d level %d%%", ticks_, level_);          // 默认看不到
    RCLCPP_DEBUG(driver_logger_, "raw voltage read");                          // 子 logger
    RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 1000, "level %d%%", level_);  // 最多 1 次/秒
    RCLCPP_WARN_EXPRESSION(get_logger(), level_ <= 30 && level_ > 10, "low battery %d%%", level_);
    if (level_ <= 10) {
      RCLCPP_ERROR_ONCE(get_logger(), "critical battery, stop now");           // 只打一次
    }
    if (level_ <= 0) {
      RCLCPP_FATAL(get_logger(), "battery empty, shutting down");
      rclcpp::shutdown();
    }
  }

  rclcpp::Logger driver_logger_ = rclcpp::get_logger("unused");
  rclcpp::TimerBase::SharedPtr timer_;
  int ticks_ = 0;
  int level_ = 50;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<BatteryMonitor>());
  return 0;
}
