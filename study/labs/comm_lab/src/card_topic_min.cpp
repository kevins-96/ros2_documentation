// 卡片《话题》的最小示例：控制器发布 /cmd_vel，底盘驱动订阅 /cmd_vel
// 适用：ROS 2 Rolling（rclcpp）
#include <chrono>
#include <memory>

#include "geometry_msgs/msg/twist.hpp"
#include "rclcpp/rclcpp.hpp"

using namespace std::chrono_literals;

class Controller : public rclcpp::Node
{
public:
  Controller()
  : Node("controller")
  {
    // 队列深度 10：订阅端处理不过来时最多缓存 10 条（KeepLast）
    pub_ = create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);
    timer_ = create_wall_timer(50ms, [this]() {
        geometry_msgs::msg::Twist cmd;
        cmd.linear.x = 0.2;
        cmd.angular.z = 0.1;
        pub_->publish(cmd);
      });
  }

private:
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

class BaseDriver : public rclcpp::Node
{
public:
  BaseDriver()
  : Node("base_driver")
  {
    // 回调参数用 const SharedPtr &：不拷贝消息
    sub_ = create_subscription<geometry_msgs::msg::Twist>(
      "cmd_vel", 10,
      [this](const geometry_msgs::msg::Twist::ConstSharedPtr & msg) {
        RCLCPP_INFO_THROTTLE(
          get_logger(), *get_clock(), 1000, "v=%.2f w=%.2f", msg->linear.x, msg->angular.z);
      });
  }

private:
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr sub_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::executors::SingleThreadedExecutor exec;
  auto controller = std::make_shared<Controller>();
  auto driver = std::make_shared<BaseDriver>();
  exec.add_node(controller);
  exec.add_node(driver);
  exec.spin();
  rclcpp::shutdown();
  return 0;
}
