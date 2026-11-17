// 卡片《执行器》的最小示例：一个执行器、两个节点、两个线程
// 适用：ROS 2 Rolling（rclcpp）
#include <chrono>
#include <memory>
#include <thread>

#include "rclcpp/rclcpp.hpp"

using namespace std::chrono_literals;

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);

  auto controller = std::make_shared<rclcpp::Node>("controller");
  auto planner = std::make_shared<rclcpp::Node>("planner");

  // 50 Hz 控制环：只打印自己跑在哪个线程
  auto control_timer = controller->create_wall_timer(20ms, [controller]() {
      RCLCPP_INFO_THROTTLE(
        controller->get_logger(), *controller->get_clock(), 1000,
        "control tick on thread %zu",
        std::hash<std::thread::id>{}(std::this_thread::get_id()));
    });

  // 1 Hz "规划"：每次占用 300 ms
  auto plan_timer = planner->create_wall_timer(1s, [planner]() {
      RCLCPP_INFO(planner->get_logger(), "planning on thread %zu ...",
      std::hash<std::thread::id>{}(std::this_thread::get_id()));
      std::this_thread::sleep_for(300ms);
    });

  // 两个节点各有自己的默认回调组 -> 属于不同组 -> 多线程执行器下可以并行
  rclcpp::executors::MultiThreadedExecutor executor(rclcpp::ExecutorOptions(), 2);
  executor.add_node(controller);
  executor.add_node(planner);
  executor.spin();  // 阻塞当前线程，直到 Ctrl-C

  rclcpp::shutdown();
  return 0;
}
