// 第 9 批：核对 055_ros_parameter_design 的说法。
// planner 节点声明几个参数（带范围、带校验回调），另一个节点 watcher 订阅参数事件，
// 依次做：未声明参数、原子批量设置、非原子批量设置、范围检查、校验回调、参数事件、参数服务名。
// 加 --serve：只声明参数并 spin，给命令行练习用（ros2 param set ... --atomic）。
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "rcl_interfaces/msg/parameter_event.hpp"
#include "rclcpp/rclcpp.hpp"

using namespace std::chrono_literals;
using rclcpp::Parameter;

static void show(const char * what, const rcl_interfaces::msg::SetParametersResult & r)
{
  std::printf("  %-44s -> %s %s\n", what, r.successful ? "成功" : "拒绝", r.reason.c_str());
}

static void show_all(const char * what, const std::vector<rcl_interfaces::msg::SetParametersResult> & rs)
{
  std::printf("  %-44s ->", what);
  for (const auto & r : rs) {
    std::printf(" [%s%s%s]", r.successful ? "成功" : "拒绝", r.reason.empty() ? "" : ": ", r.reason.c_str());
  }
  std::printf("\n");
}

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto planner = std::make_shared<rclcpp::Node>("planner", "/robot1");
  auto watcher = std::make_shared<rclcpp::Node>("watcher", "/robot1");

  // 1) 声明：max_speed 带范围 [0, 2]，步长 0（连续）
  rcl_interfaces::msg::ParameterDescriptor speed_desc;
  speed_desc.description = "最大线速度 m/s";
  rcl_interfaces::msg::FloatingPointRange range;
  range.from_value = 0.0;
  range.to_value = 2.0;
  range.step = 0.0;
  speed_desc.floating_point_range.push_back(range);
  planner->declare_parameter("max_speed", 1.0, speed_desc);
  planner->declare_parameter("max_accel", 0.5);
  planner->declare_parameter("frame_id", std::string("map"));
  planner->declare_parameter("mask", std::vector<bool>{true, false});
  planner->declare_parameter("blob", std::vector<uint8_t>{0x01, 0x02});

  // 2) 校验回调：max_accel 必须小于 max_speed（跨参数约束，范围描述表达不了）
  auto validate = planner->add_on_set_parameters_callback(
    [planner](const std::vector<Parameter> & ps) {
      rcl_interfaces::msg::SetParametersResult r;
      r.successful = true;
      double speed = planner->get_parameter("max_speed").as_double();
      double accel = planner->get_parameter("max_accel").as_double();
      for (const auto & p : ps) {
        if (p.get_name() == "max_speed") {speed = p.as_double();}
        if (p.get_name() == "max_accel") {accel = p.as_double();}
      }
      if (accel >= speed) {
        r.successful = false;
        r.reason = "max_accel 必须小于 max_speed";
      }
      return r;
    });

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--serve") == 0) {
      rclcpp::spin(planner);
      rclcpp::shutdown();
      return 0;
    }
  }

  // 3) watcher 订阅参数事件，统计收到哪些
  int events = 0;
  auto sub = watcher->create_subscription<rcl_interfaces::msg::ParameterEvent>(
    "/parameter_events", rclcpp::ParametersQoS(),
    [&events](rcl_interfaces::msg::ParameterEvent::ConstSharedPtr e) {
      if (e->node != "/robot1/planner") {return;}
      ++events;
      std::printf("    [事件] node=%s new=%zu changed=%zu deleted=%zu:", e->node.c_str(),
      e->new_parameters.size(), e->changed_parameters.size(), e->deleted_parameters.size());
      for (const auto & p : e->changed_parameters) {std::printf(" %s", p.name.c_str());}
      for (const auto & p : e->new_parameters) {std::printf(" +%s", p.name.c_str());}
      for (const auto & p : e->deleted_parameters) {std::printf(" -%s", p.name.c_str());}
      std::printf("\n");
    });
  // 4) 对单个参数注册回调（设计文档写的"未实现"）
  auto handler = std::make_shared<rclcpp::ParameterEventHandler>(watcher);
  auto cb = handler->add_parameter_callback(
    "max_speed", [](const Parameter & p) {
      std::printf("    [单参数回调] max_speed -> %.2f\n", p.as_double());
    }, "/robot1/planner");

  rclcpp::executors::SingleThreadedExecutor exec;
  exec.add_node(watcher);
  std::thread spin([&exec] {exec.spin();});
  std::this_thread::sleep_for(500ms);
  auto settle = [] {std::this_thread::sleep_for(300ms);};

  std::printf("A. 参数服务（节点自带）:\n");
  for (const auto & [name, types] : planner->get_service_names_and_types_by_node("planner", "/robot1")) {
    std::printf("  %s\n", name.c_str());
  }

  std::printf("B. 设置未声明的参数:\n");
  try {
    show("set max_jerk=1.0（未声明）", planner->set_parameter(Parameter("max_jerk", 1.0)));
  } catch (const std::exception & e) {
    std::printf("  set max_jerk=1.0 抛异常: %s\n", e.what());
  }
  settle();

  std::printf("C. 范围检查（描述里的 floating_point_range）:\n");
  show("set max_speed=3.0（超出 [0,2]）", planner->set_parameter(Parameter("max_speed", 3.0)));
  settle();

  std::printf("D. 原子批量设置（max_speed=1.5 合法，max_accel=2.0 违反回调）:\n");
  show("set_parameters_atomically", planner->set_parameters_atomically(
    {Parameter("max_speed", 1.5), Parameter("max_accel", 2.0)}));
  settle();
  std::printf("  结果：max_speed=%.2f max_accel=%.2f\n",
    planner->get_parameter("max_speed").as_double(), planner->get_parameter("max_accel").as_double());

  std::printf("E. 非原子批量设置（同样两个值，逐个设置）:\n");
  show_all("set_parameters", planner->set_parameters(
    {Parameter("max_speed", 1.5), Parameter("max_accel", 2.0)}));
  settle();
  std::printf("  结果：max_speed=%.2f max_accel=%.2f\n",
    planner->get_parameter("max_speed").as_double(), planner->get_parameter("max_accel").as_double());

  std::printf("F. 被拒绝的修改会不会发事件？（上面每次拒绝后都没有 [事件] 行就说明不会）共收到 %d 条事件\n", events);

  std::printf("G. 描述参数（远端工具能看到的约束）:\n");
  for (const auto & d : planner->describe_parameters({"max_speed", "blob", "mask"})) {
    std::printf("  %s type=%u", d.name.c_str(), d.type);
    if (!d.floating_point_range.empty()) {
      std::printf(" range=[%.1f, %.1f]", d.floating_point_range[0].from_value, d.floating_point_range[0].to_value);
    }
    std::printf(" desc=\"%s\"\n", d.description.c_str());
  }

  std::printf("H. 嵌套参数名用 '.' 分隔:\n");
  planner->declare_parameter("controller.kp", 1.0);
  for (const auto & n : planner->list_parameters({"controller"}, 0).names) {
    std::printf("  %s\n", n.c_str());
  }
  settle();

  exec.cancel();
  spin.join();
  (void)validate;
  (void)cb;
  rclcpp::shutdown();
  return 0;
}
