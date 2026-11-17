// 练习「进程内通信与零拷贝」：借用消息（loaned message）在当前中间件上能不能用
// 运行：ros2 run perf_lab loan_lab
//       ROS_DISABLE_LOANED_MESSAGES=1 ros2 run perf_lab loan_lab
#include <cstdio>
#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/float64.hpp"
#include "std_msgs/msg/string.hpp"

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<rclcpp::Node>("loan_lab");
  auto pod_pub = node->create_publisher<std_msgs::msg::Float64>("pod", 10);       // 定长：POD
  auto str_pub = node->create_publisher<std_msgs::msg::String>("text", 10);       // 变长：非 POD

  std::printf("rmw: %s\n", rmw_get_implementation_identifier());
  std::printf("Float64 can_loan_messages: %s\n", pod_pub->can_loan_messages() ? "true" : "false");
  std::printf("String  can_loan_messages: %s\n", str_pub->can_loan_messages() ? "true" : "false");

  for (int i = 0; i < 2; ++i) {
    auto pod = pod_pub->borrow_loaned_message();     // 借：中间件支持时内存归中间件
    pod.get().data = 1.0 * i;
    std::printf("Float64 loan #%d at %p\n", i, static_cast<void *>(&pod.get()));
    pod_pub->publish(std::move(pod));                // 发布后借来的消息就不能再用了

    auto str = str_pub->borrow_loaned_message();     // 不支持时退回到发布者自己的分配器
    str.get().data = "hello " + std::to_string(i);
    str_pub->publish(std::move(str));
  }
  rclcpp::shutdown();
  return 0;
}
