// 卡片「生命周期节点」最小代码：一个"激光驱动"
//   configure：读参数、创建发布者和定时器（还不发数据）
//   activate ：开始发数据
//   deactivate / cleanup / shutdown：按相反顺序释放
// 运行：ros2 run config_lab card_lifecycle_min
//       ros2 lifecycle set /lidar_driver configure   （再 activate、deactivate、cleanup、shutdown）
// 实验参数：
//   -p fail_configure:=true      on_configure 返回 FAILURE
//   -p error_configure:=true     on_configure 返回 ERROR（进入错误处理）
//   -p skip_base_activate:=true  重写 on_activate 时忘了调用基类，发布者没有被激活
#include <chrono>
#include <memory>
#include <string>

#include "lifecycle_msgs/msg/state.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_lifecycle/lifecycle_node.hpp"
#include "rclcpp_lifecycle/lifecycle_publisher.hpp"
#include "std_msgs/msg/string.hpp"

using namespace std::chrono_literals;
using CallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;
using rclcpp_lifecycle::State;

class LidarDriver : public rclcpp_lifecycle::LifecycleNode
{
public:
  LidarDriver()
  : LifecycleNode("lidar_driver")
  {
    // 参数在构造时声明（任何状态都能查到），在 configure 时读取
    declare_parameter("fail_configure", false);
    declare_parameter("error_configure", false);
    declare_parameter("skip_base_activate", false);
    declare_parameter("rate_hz", 2.0);
  }

  CallbackReturn on_configure(const State &) override
  {
    if (get_parameter("error_configure").as_bool()) {
      RCLCPP_ERROR(get_logger(), "on_configure: device crashed");
      return CallbackReturn::ERROR;
    }
    if (get_parameter("fail_configure").as_bool()) {
      RCLCPP_WARN(get_logger(), "on_configure: device not found");
      return CallbackReturn::FAILURE;
    }
    pub_ = create_publisher<std_msgs::msg::String>("scan_status", 10);
    const auto period = std::chrono::duration<double>(1.0 / get_parameter("rate_hz").as_double());
    timer_ = create_wall_timer(period, [this]() {
        std_msgs::msg::String m;
        m.data = "scan " + std::to_string(count_++);
        pub_->publish(m);   // 未激活时：不发送，只打警告
      });
    RCLCPP_INFO(get_logger(), "on_configure: ok");
    return CallbackReturn::SUCCESS;
  }

  CallbackReturn on_activate(const State & state) override
  {
    if (!get_parameter("skip_base_activate").as_bool()) {
      LifecycleNode::on_activate(state);   // 基类负责激活本节点的所有 LifecyclePublisher
    }
    RCLCPP_INFO(get_logger(), "on_activate: publisher activated=%s",
      pub_->is_activated() ? "true" : "false");
    return CallbackReturn::SUCCESS;
  }

  CallbackReturn on_deactivate(const State & state) override
  {
    LifecycleNode::on_deactivate(state);
    RCLCPP_INFO(get_logger(), "on_deactivate");
    return CallbackReturn::SUCCESS;
  }

  CallbackReturn on_cleanup(const State &) override
  {
    timer_.reset();
    pub_.reset();
    RCLCPP_INFO(get_logger(), "on_cleanup: resources released");
    return CallbackReturn::SUCCESS;
  }

  CallbackReturn on_shutdown(const State & state) override
  {
    timer_.reset();
    pub_.reset();
    RCLCPP_INFO(get_logger(), "on_shutdown from %s", state.label().c_str());
    return CallbackReturn::SUCCESS;
  }

  CallbackReturn on_error(const State & state) override
  {
    RCLCPP_ERROR(get_logger(), "on_error from %s, resetting", state.label().c_str());
    timer_.reset();
    pub_.reset();
    return CallbackReturn::SUCCESS;   // SUCCESS -> 回到 unconfigured；FAILURE -> finalized
  }

private:
  rclcpp_lifecycle::LifecyclePublisher<std_msgs::msg::String>::SharedPtr pub_;
  rclcpp::TimerBase::SharedPtr timer_;
  int count_ = 0;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<LidarDriver>();
  // LifecycleNode 不是 rclcpp::Node 的子类，要交给执行器的是 get_node_base_interface()
  rclcpp::spin(node->get_node_base_interface());
  rclcpp::shutdown();
  return 0;
}
