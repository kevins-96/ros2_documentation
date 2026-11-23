// 第 9 批：核对 node_lifecycle（Managed nodes）的说法。
// 模式 1（默认）：在进程内用本地 API 驱动一个生命周期节点，测"回调返回 FAILURE / 抛异常 / on_error 失败"
//                 分别把节点带到哪个状态；再列出管理接口（服务、话题）和它们的 QoS。
// 模式 2（--serve）：只创建节点 lidar_driver 然后 spin，给 launch 练习当被管理的节点。
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include "lifecycle_msgs/msg/state.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_lifecycle/lifecycle_node.hpp"
#include "std_msgs/msg/string.hpp"

using namespace std::chrono_literals;
using CallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

// 每个回调的行为由成员变量控制，方便一个进程里试多种情形
class Driver : public rclcpp_lifecycle::LifecycleNode
{
public:
  explicit Driver(const std::string & name)
  : LifecycleNode(name) {}

  std::string activate_mode = "ok";   // ok | failure | error | throw
  std::string error_mode = "ok";      // on_error 的返回：ok | failure

  CallbackReturn on_configure(const rclcpp_lifecycle::State &) override
  {
    pub_ = create_publisher<std_msgs::msg::String>("status", 10);
    RCLCPP_INFO(get_logger(), "on_configure");
    return CallbackReturn::SUCCESS;
  }
  CallbackReturn on_activate(const rclcpp_lifecycle::State & s) override
  {
    RCLCPP_INFO(get_logger(), "on_activate (mode=%s)", activate_mode.c_str());
    if (activate_mode == "failure") {return CallbackReturn::FAILURE;}
    if (activate_mode == "error") {return CallbackReturn::ERROR;}
    if (activate_mode == "throw") {throw std::runtime_error("lidar not responding");}
    return LifecycleNode::on_activate(s);
  }
  CallbackReturn on_deactivate(const rclcpp_lifecycle::State & s) override
  {
    RCLCPP_INFO(get_logger(), "on_deactivate");
    return LifecycleNode::on_deactivate(s);
  }
  CallbackReturn on_cleanup(const rclcpp_lifecycle::State &) override
  {
    RCLCPP_INFO(get_logger(), "on_cleanup");
    pub_.reset();
    return CallbackReturn::SUCCESS;
  }
  CallbackReturn on_shutdown(const rclcpp_lifecycle::State & s) override
  {
    RCLCPP_INFO(get_logger(), "on_shutdown (from %s)", s.label().c_str());
    return CallbackReturn::SUCCESS;
  }
  CallbackReturn on_error(const rclcpp_lifecycle::State & s) override
  {
    RCLCPP_INFO(get_logger(), "on_error (from %s, mode=%s)", s.label().c_str(), error_mode.c_str());
    return error_mode == "ok" ? CallbackReturn::SUCCESS : CallbackReturn::FAILURE;
  }

private:
  rclcpp_lifecycle::LifecyclePublisher<std_msgs::msg::String>::SharedPtr pub_;
};

static const char * label(const rclcpp_lifecycle::State & s)
{
  static std::string buf;
  buf = s.label();
  return buf.c_str();
}

static void run_case(const char * title, const std::string & activate_mode, const std::string & error_mode)
{
  auto n = std::make_shared<Driver>("lidar_driver");
  n->activate_mode = activate_mode;
  n->error_mode = error_mode;
  CallbackReturn rc;
  n->configure(rc);
  auto after = n->activate(rc);
  std::printf("%-46s activate 后状态 = %-12s (返回码 %d)\n", title, label(after), static_cast<int>(rc));
  if (after.id() == lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE) {
    n->shutdown(rc);
  }
}

int main(int argc, char ** argv)
{
  bool serve = false;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--serve") == 0) {serve = true;}
  }
  rclcpp::init(argc, argv);

  if (serve) {
    auto n = std::make_shared<Driver>("lidar_driver");
    rclcpp::spin(n->get_node_base_interface());
    rclcpp::shutdown();
    return 0;
  }

  std::printf("A. 回调结果把节点带到哪里:\n");
  run_case("on_activate 返回 SUCCESS", "ok", "ok");
  run_case("on_activate 返回 FAILURE", "failure", "ok");
  run_case("on_activate 返回 ERROR，on_error 成功", "error", "ok");
  run_case("on_activate 抛异常，on_error 成功", "throw", "ok");
  run_case("on_activate 抛异常，on_error 返回 FAILURE", "throw", "failure");

  std::printf("B. 管理接口（服务、话题）:\n");
  auto n = std::make_shared<Driver>("lidar_driver");
  auto probe = std::make_shared<rclcpp::Node>("probe");
  std::this_thread::sleep_for(800ms);
  for (const auto & [name, types] : probe->get_service_names_and_types_by_node("lidar_driver", "/")) {
    if (name.find("param") != std::string::npos || name.find("type_description") != std::string::npos) {continue;}
    std::printf("  服务 %-40s %s\n", name.c_str(), types.front().c_str());
  }
  for (const auto & info : probe->get_publishers_info_by_topic("/lidar_driver/transition_event")) {
    auto q = info.qos_profile();
    std::printf("  话题 /lidar_driver/transition_event  %s  depth=%zu reliability=%s durability=%s\n",
      info.topic_type().c_str(), q.depth(),
      q.reliability() == rclcpp::ReliabilityPolicy::Reliable ? "reliable" : "best_effort",
      q.durability() == rclcpp::DurabilityPolicy::TransientLocal ? "transient_local" : "volatile");
  }

  std::printf("C. 从各个主状态能不能直接 shutdown:\n");
  for (int start = 0; start < 3; ++start) {
    auto m = std::make_shared<Driver>("lidar_driver_" + std::to_string(start));
    CallbackReturn rc;
    if (start >= 1) {m->configure(rc);}
    if (start >= 2) {m->activate(rc);}
    auto from = m->get_current_state().label();
    auto after = m->shutdown(rc);
    std::printf("  从 %-12s shutdown -> %s\n", from.c_str(), label(after));
    // 先把 rc 设成一个"哨兵"值，看非法转换时它会不会被改写
    rc = static_cast<CallbackReturn>(-1);
    auto again = m->configure(rc);
    std::printf("  再 configure -> %s (返回码 %d；CallbackReturn 是 uint8，255 表示没被改写)\n", label(again), static_cast<int>(rc));
  }
  rclcpp::shutdown();
  return 0;
}
