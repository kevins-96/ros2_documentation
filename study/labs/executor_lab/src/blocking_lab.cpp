// 实验 A：控制定时器会不会被"慢规划服务"拖慢？
//
// 一个节点里放两个回调：
//   - control 定时器：50 Hz（20 ms），模拟控制环，统计相邻两次触发的实际间隔
//   - /plan 服务：每次处理 sleep 300 ms，模拟一次全局路径规划
//
// 命令行参数：
//   --executor single|multi       使用 SingleThreadedExecutor 或 MultiThreadedExecutor(2 线程)
//   --groups   default|separate   两个回调都放节点默认组，或各放一个独立的互斥回调组
//
// 另开终端持续调用服务制造负载：
//   ros2 service call /plan std_srvs/srv/Trigger -r 2
//
// 适用：ROS 2 Rolling（rclcpp）

#include <algorithm>
#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include <thread>

#include "rclcpp/rclcpp.hpp"
#include "std_srvs/srv/trigger.hpp"

using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;

class BlockingLab : public rclcpp::Node
{
public:
  explicit BlockingLab(bool separate_groups)
  : Node("blocking_lab")
  {
    // nullptr 表示"使用节点的默认回调组"（默认组是互斥组）
    rclcpp::CallbackGroup::SharedPtr control_group = nullptr;
    rclcpp::CallbackGroup::SharedPtr plan_group = nullptr;
    if (separate_groups) {
      // 回调组必须被持有（这里存成成员），否则执行器不会调用其中的回调
      control_group_ = create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);
      plan_group_ = create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);
      control_group = control_group_;
      plan_group = plan_group_;
    }

    control_timer_ = create_wall_timer(20ms, [this]() {on_control();}, control_group);

    plan_srv_ = create_service<std_srvs::srv::Trigger>(
      "plan",
      [this](
        const std::shared_ptr<std_srvs::srv::Trigger::Request>,
        std::shared_ptr<std_srvs::srv::Trigger::Response> res)
      {
        RCLCPP_INFO(get_logger(), "plan: start (300 ms)");
        std::this_thread::sleep_for(300ms);  // 模拟耗时规划
        res->success = true;
        RCLCPP_INFO(get_logger(), "plan: done");
      },
      rclcpp::ServicesQoS(), plan_group);

    window_start_ = last_tick_ = Clock::now();
  }

private:
  void on_control()
  {
    const auto now = Clock::now();
    const double dt_ms = std::chrono::duration<double, std::milli>(now - last_tick_).count();
    last_tick_ = now;
    max_dt_ms_ = std::max(max_dt_ms_, dt_ms);
    if (dt_ms > 30.0) {
      ++late_ticks_;  // 超过周期 1.5 倍，视为"漏拍"
    }

    if (now - window_start_ >= 2s) {
      RCLCPP_INFO(
        get_logger(), "control: max period %.1f ms (expected 20.0), late ticks %d",
        max_dt_ms_, late_ticks_);
      window_start_ = now;
      max_dt_ms_ = 0.0;
      late_ticks_ = 0;
    }
  }

  rclcpp::CallbackGroup::SharedPtr control_group_;
  rclcpp::CallbackGroup::SharedPtr plan_group_;
  rclcpp::TimerBase::SharedPtr control_timer_;
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr plan_srv_;

  Clock::time_point window_start_;
  Clock::time_point last_tick_;
  double max_dt_ms_{0.0};
  int late_ticks_{0};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);

  std::string executor_type = "single";
  std::string groups = "default";
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::strcmp(argv[i], "--executor") == 0) {executor_type = argv[i + 1];}
    if (std::strcmp(argv[i], "--groups") == 0) {groups = argv[i + 1];}
  }

  auto node = std::make_shared<BlockingLab>(groups == "separate");
  RCLCPP_INFO(
    node->get_logger(), "executor=%s groups=%s", executor_type.c_str(), groups.c_str());

  if (executor_type == "multi") {
    rclcpp::executors::MultiThreadedExecutor exec(rclcpp::ExecutorOptions(), 2);
    exec.add_node(node);
    exec.spin();
  } else {
    rclcpp::executors::SingleThreadedExecutor exec;
    exec.add_node(node);
    exec.spin();
  }

  rclcpp::shutdown();
  return 0;
}
