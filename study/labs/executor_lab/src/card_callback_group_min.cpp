// 卡片《回调组》的最小示例：同一个节点里，让控制环不被规划服务阻塞
// 适用：ROS 2 Rolling（rclcpp）
#include <chrono>
#include <memory>
#include <thread>

#include "rclcpp/rclcpp.hpp"
#include "std_srvs/srv/trigger.hpp"

using namespace std::chrono_literals;

class NavNode : public rclcpp::Node
{
public:
  NavNode()
  : Node("nav_node")
  {
    // 1. 建两个互斥组，并存成成员（不持有的话回调不会被执行）
    control_group_ = create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);
    plan_group_ = create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);

    // 2. 创建实体时把组传进去
    control_timer_ = create_wall_timer(
      20ms, [this]() {/* 计算并发布 cmd_vel */}, control_group_);

    plan_srv_ = create_service<std_srvs::srv::Trigger>(
      "plan",
      [](const std::shared_ptr<std_srvs::srv::Trigger::Request>,
      std::shared_ptr<std_srvs::srv::Trigger::Response> res) {
        std::this_thread::sleep_for(300ms);  // 耗时规划
        res->success = true;
      },
      rclcpp::ServicesQoS(), plan_group_);
  }

private:
  rclcpp::CallbackGroup::SharedPtr control_group_;
  rclcpp::CallbackGroup::SharedPtr plan_group_;
  rclcpp::TimerBase::SharedPtr control_timer_;
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr plan_srv_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<NavNode>();
  // 3. 必须用多线程执行器，不同组之间才可能真正并行
  rclcpp::executors::MultiThreadedExecutor executor(rclcpp::ExecutorOptions(), 2);
  executor.add_node(node);
  executor.spin();
  rclcpp::shutdown();
  return 0;
}
