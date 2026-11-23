// 第 9 批：核对 141_static_remapping / 160_ros_command_line_arguments 的说法。
// 一个进程里放两个节点 planner、controller（都在 /robot1），打印它们用到的名字被重映射成了什么。
// 用法：ros2 run design_lab remap_probe [自己的参数...] --ros-args <重映射规则...> [-- 自己的参数...]
//   例：ros2 run design_lab remap_probe --ros-args -r planner:scan:=scan_filtered
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include "std_msgs/msg/string.hpp"
#include "study_interfaces/action/move_to.hpp"
#include "study_interfaces/srv/compute_path.hpp"

using MoveTo = study_interfaces::action::MoveTo;
using namespace std::chrono_literals;

static void print_names(const rclcpp::Node::SharedPtr & node)
{
  auto topics = node->get_node_topics_interface();
  auto services = node->get_node_services_interface();
  std::printf("[%s] 节点全名 = %s\n", node->get_name(), node->get_fully_qualified_name());
  for (const auto & pname : node->list_parameters({}, 0).names) {
    if (pname == "use_sim_time" || pname.rfind("qos_overrides", 0) == 0 || pname.rfind("start_type_description_service", 0) == 0) {continue;}
    std::printf("  参数 %s = %s\n", pname.c_str(), node->get_parameter(pname).value_to_string().c_str());
  }
  // 代码里写的名字 -> 话题解析结果 / 服务解析结果
  const std::vector<std::string> names = {"scan", "/scan/head/scan", "map", "~/status", "/diagnostics"};
  for (const auto & n : names) {
    std::string as_topic, as_service;
    try {
      as_topic = topics->resolve_topic_name(n);
    } catch (const std::exception & e) {
      as_topic = std::string("<异常: ") + e.what() + ">";
    }
    try {
      as_service = services->resolve_service_name(n);
    } catch (const std::exception & e) {
      as_service = std::string("<异常: ") + e.what() + ">";
    }
    std::printf("  %-16s 话题 -> %-32s 服务 -> %s\n", n.c_str(), as_topic.c_str(), as_service.c_str());
  }
}

int main(int argc, char ** argv)
{
  // 先看 rclcpp 把哪些参数留给了用户（--ros-args ... -- 之外的部分）
  std::vector<std::string> user_args;
  try {
    user_args = rclcpp::init_and_remove_ros_arguments(argc, argv);
  } catch (const std::exception & e) {
    std::printf("init 失败: %s\n", e.what());
    return 1;
  }
  std::printf("留给用户的参数:");
  for (size_t i = 1; i < user_args.size(); ++i) {
    std::printf(" [%s]", user_args[i].c_str());
  }
  std::printf("\n");

  std::vector<rclcpp::Node::SharedPtr> nodes;
  for (const char * name : {"planner", "controller"}) {
    try {
      // 自动把命令行/YAML 传进来的参数声明出来，方便看 -p 和 --params-file 的通配符落到了哪个节点
      nodes.push_back(std::make_shared<rclcpp::Node>(
          name, "/robot1",
          rclcpp::NodeOptions().automatically_declare_parameters_from_overrides(true)));
    } catch (const std::exception & e) {
      std::printf("创建节点 %s 失败: %s\n", name, e.what());
      rclcpp::shutdown();
      return 1;
    }
  }
  for (const auto & n : nodes) {
    print_names(n);
  }

  // 动作名的重映射：planner 上建一个动作服务端 move_to，看图里出现的隐藏话题/服务名
  auto server = rclcpp_action::create_server<MoveTo>(
    nodes[0], "move_to",
    [](const rclcpp_action::GoalUUID &, std::shared_ptr<const MoveTo::Goal>) {
      return rclcpp_action::GoalResponse::REJECT;
    },
    [](std::shared_ptr<rclcpp_action::ServerGoalHandle<MoveTo>>) {
      return rclcpp_action::CancelResponse::REJECT;
    },
    [](std::shared_ptr<rclcpp_action::ServerGoalHandle<MoveTo>>) {});
  // 同名的话题和服务：map 既是话题又是服务，用来测 rostopic:// / rosservice:// 只改一种
  auto pub = nodes[0]->create_publisher<std_msgs::msg::String>("map", 1);
  auto srv = nodes[0]->create_service<study_interfaces::srv::ComputePath>(
    "map", [](const std::shared_ptr<study_interfaces::srv::ComputePath::Request>,
    std::shared_ptr<study_interfaces::srv::ComputePath::Response>) {});

  std::this_thread::sleep_for(500ms);
  std::printf("planner 上实际创建的实体:\n");
  std::printf("  发布者 map -> %s\n", pub->get_topic_name());
  std::printf("  服务 map -> %s\n", srv->get_service_name());
  for (const auto & [name, types] :
    nodes[0]->get_node_graph_interface()->get_publisher_names_and_types_by_node(nodes[0]->get_name(), nodes[0]->get_namespace()))
  {
    if (types.front().find("action") != std::string::npos) {
      std::printf("  动作话题 %s\n", name.c_str());
    }
  }
  for (const auto & [name, types] : nodes[0]->get_service_names_and_types_by_node(nodes[0]->get_name(), nodes[0]->get_namespace())) {
    if (name.find("_action") != std::string::npos) {
      std::printf("  动作服务 %s\n", name.c_str());
    }
  }
  rclcpp::shutdown();
  return 0;
}
