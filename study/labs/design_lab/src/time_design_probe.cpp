// 第 9 批：核对 130_ros_time（Clock and Time）的说法。
// 一个 use_sim_time=true 的节点 + 一个自己发 /clock 的节点，在同一个进程里：
//   A. 还没收到 /clock 时 now() 是多少；B. /clock 往回跳时跳变回调怎么报；
//   C. ROS 时钟定时器和墙钟定时器在 /clock 暂停时的表现；D. 不同时钟类型的时间能不能比较；
//   E. rclcpp::Rate 默认用哪个时钟；F. /clock 订阅的 QoS。
#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>

#include "rclcpp/rclcpp.hpp"
#include "rosgraph_msgs/msg/clock.hpp"

using namespace std::chrono_literals;

static double sec(const rclcpp::Time & t) {return t.seconds();}

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto sim = std::make_shared<rclcpp::Node>(
    "planner", rclcpp::NodeOptions().parameter_overrides({{"use_sim_time", true}}));
  auto sim_clock_src = std::make_shared<rclcpp::Node>("fake_sim");
  auto clock_pub = sim_clock_src->create_publisher<rosgraph_msgs::msg::Clock>("/clock", rclcpp::ClockQoS());

  rclcpp::executors::MultiThreadedExecutor exec;
  exec.add_node(sim);
  exec.add_node(sim_clock_src);
  std::thread spin([&exec] {exec.spin();});

  auto publish_clock = [&clock_pub](double t) {
    rosgraph_msgs::msg::Clock c;
    c.clock = rclcpp::Time(static_cast<int64_t>(t * 1e9), RCL_ROS_TIME);
    clock_pub->publish(c);
  };

  std::printf("A. 收到 /clock 之前: now() = %.3f s, clock 类型 = %d (RCL_ROS_TIME=%d), ros_time_is_active = %d\n",
    sec(sim->now()), sim->get_clock()->get_clock_type(), RCL_ROS_TIME,
    sim->get_clock()->ros_time_is_active());

  // B. 跳变回调：向前跳超过 1 s、向后跳任意距离、时钟类型变化都报。
  //    注意：如果 min_forward 设得很小，/clock 每更新一次都会触发一次"向前跳变"。
  rcl_jump_threshold_t th;
  th.on_clock_change = true;
  th.min_forward.nanoseconds = RCL_S_TO_NS(1);
  th.min_backward.nanoseconds = -1;
  auto jump = sim->get_clock()->create_jump_callback(
    [&sim]() {std::printf("    [跳变前回调] 此时 now() = %.3f\n", sec(sim->now()));},
    [&sim](const rcl_time_jump_t & j) {
      const char * kind = j.clock_change == RCL_ROS_TIME_NO_CHANGE ? "时间跳变" :
      j.clock_change == RCL_ROS_TIME_ACTIVATED ? "ROS 时间被激活" :
      j.clock_change == RCL_ROS_TIME_DEACTIVATED ? "ROS 时间被关闭" : "时钟类型变化";
      std::printf("    [跳变后回调] %s, delta = %.3f s, 此时 now() = %.3f\n", kind,
      j.delta.nanoseconds / 1e9, sec(sim->now()));
    }, th);

  // C. 两个定时器：ROS 时钟 0.5 s、墙钟 0.5 s
  int ros_ticks = 0, wall_ticks = 0;
  auto t_ros = sim->create_timer(500ms, [&ros_ticks] {++ros_ticks;});
  auto t_wall = sim->create_wall_timer(500ms, [&wall_ticks] {++wall_ticks;});

  std::printf("B. /clock 发布 100 s，然后往回跳到 40 s:\n");
  std::this_thread::sleep_for(500ms);
  publish_clock(100.0);
  std::this_thread::sleep_for(300ms);
  std::printf("  now() = %.3f\n", sec(sim->now()));
  publish_clock(40.0);
  std::this_thread::sleep_for(300ms);
  std::printf("  now() = %.3f\n", sec(sim->now()));

  std::printf("C. /clock 以 10 倍速推进 1 s 墙钟（仿真走 10 s），再暂停 2 s 墙钟:\n");
  ros_ticks = wall_ticks = 0;
  double t = 40.0;
  for (int i = 0; i < 100; ++i) {
    t += 0.1;
    publish_clock(t);
    std::this_thread::sleep_for(10ms);
  }
  std::printf("  推进阶段: ROS 定时器 %d 次, 墙钟定时器 %d 次\n", ros_ticks, wall_ticks);
  ros_ticks = wall_ticks = 0;
  std::this_thread::sleep_for(2s);
  std::printf("  暂停阶段: ROS 定时器 %d 次, 墙钟定时器 %d 次\n", ros_ticks, wall_ticks);

  std::printf("D. 不同时钟类型的时间能不能比较:\n");
  rclcpp::Time t_steady(1, 0, RCL_STEADY_TIME), t_system(1, 0, RCL_SYSTEM_TIME), t_rosx(1, 0, RCL_ROS_TIME);
  auto try_cmp = [](const char * what, const rclcpp::Time & a, const rclcpp::Time & b) {
      try {
        bool r = a < b;
        std::printf("  %-24s 编译通过，运行结果 %d\n", what, r);
      } catch (const std::exception & e) {
        std::printf("  %-24s 编译通过，运行时抛异常: %s\n", what, e.what());
      }
    };
  try_cmp("steady < ros", t_steady, t_rosx);
  try_cmp("system < ros", t_system, t_rosx);
  try_cmp("ros < ros", t_rosx, t_rosx);

  std::printf("E. rclcpp::Rate 默认时钟:\n");
  rclcpp::Rate r(10.0);
  std::printf("  Rate(10) 的时钟类型 = %d (SYSTEM=%d, ROS=%d)\n", r.get_type(), RCL_SYSTEM_TIME, RCL_ROS_TIME);
  rclcpp::Rate r_sim(10.0, sim->get_clock());
  std::printf("  Rate(10, node clock) 的时钟类型 = %d\n", r_sim.get_type());

  std::printf("F. /clock 的 QoS:\n");
  for (const auto & info : sim->get_subscriptions_info_by_topic("/clock")) {
    auto q = info.qos_profile();
    std::printf("  订阅者 %s: depth=%zu reliability=%s\n", info.node_name().c_str(), q.depth(),
      q.reliability() == rclcpp::ReliabilityPolicy::Reliable ? "reliable" : "best_effort");
  }

  std::printf("G. 关掉 use_sim_time:\n");
  sim->set_parameter(rclcpp::Parameter("use_sim_time", false));
  std::this_thread::sleep_for(200ms);
  std::printf("  now() = %.0f s（墙钟）\n", sec(sim->now()));

  exec.cancel();
  spin.join();
  (void)jump;
  rclcpp::shutdown();
  return 0;
}
