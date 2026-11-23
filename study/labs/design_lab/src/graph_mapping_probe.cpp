// 练习：ROS 图是怎么"落"到 DDS 上的？
//
// 设计文档：Topic and Service name mapping to DDS、Node to Participant mapping
// 一个进程里放两个节点（同一个 context），然后：
//   1. 打印各种写法的名字展开成什么（相对、绝对、~、{node}、隐藏名字）
//   2. 故意用非法名字，看 rclcpp 在哪一层拒绝
//   3. 测 ignore_local_publications 在"一个进程一个参与者"之后的语义
//   4. 保持运行 --hold 秒，方便另一个终端抓包或运行 participant_map
//
// 用法：ros2 run design_lab graph_mapping_probe [--hold 秒] [--separate-context]
//   --separate-context  让 controller 节点用自己的 context（于是有自己的 DDS 参与者）
// 适用：ROS 2 Rolling（rclcpp 33.x，rmw_fastrtps_cpp）
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include "std_msgs/msg/string.hpp"
#include "study_interfaces/action/move_to.hpp"
#include "study_interfaces/srv/compute_path.hpp"

using namespace std::chrono_literals;
using StringMsg = std_msgs::msg::String;
using MoveTo = study_interfaces::action::MoveTo;
using ComputePath = study_interfaces::srv::ComputePath;

namespace
{
// 试着用一个名字创建发布者，打印展开结果或异常信息
void try_name(rclcpp::Node & node, const std::string & name)
{
  try {
    auto pub = node.create_publisher<StringMsg>(name, 10);
    RCLCPP_INFO(node.get_logger(), "  %-28s -> %s", ("\"" + name + "\"").c_str(),
      pub->get_topic_name());
  } catch (const std::exception & e) {
    // 只取异常信息的第一行，避免刷屏
    std::string what = e.what();
    what = what.substr(0, what.find('\n'));
    RCLCPP_WARN(node.get_logger(), "  %-28s -> 拒绝: %s",
      ("\"" + name.substr(0, 24) + (name.size() > 24 ? "..." : "") + "\"").c_str(),
      what.substr(0, 110).c_str());
  }
}
}  // namespace

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  int hold_s = 0;
  bool separate_context = false;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--hold") == 0 && i + 1 < argc) {hold_s = std::atoi(argv[i + 1]);}
    if (std::strcmp(argv[i], "--separate-context") == 0) {separate_context = true;}
  }

  // 默认：同一个进程、同一个 context 里的两个节点
  auto planner = std::make_shared<rclcpp::Node>("planner", "/robot1");
  rclcpp::NodeOptions controller_opts;
  std::shared_ptr<rclcpp::Context> ctx2;
  if (separate_context) {
    ctx2 = std::make_shared<rclcpp::Context>();
    rclcpp::InitOptions io;
    io.auto_initialize_logging(false);  // 日志只初始化一次
    ctx2->init(argc, argv, io);
    controller_opts.context(ctx2);
  }
  auto controller = std::make_shared<rclcpp::Node>("controller", "/robot1", controller_opts);
  auto log = planner->get_logger();

  // ---------- 1. 名字展开 ----------
  RCLCPP_INFO(log, "[1] 名字展开（节点 /robot1/planner）");
  std::vector<rclcpp::PublisherBase::SharedPtr> keep;  // 保持存活，方便抓包时看到
  for (const char * n : {"plan", "/global_plan", "~/debug", "{node}/status",
      "{ns}/fleet_status", "_internal/heartbeat"})
  {
    auto pub = planner->create_publisher<StringMsg>(n, 10);
    RCLCPP_INFO(log, "  %-28s -> %s", (std::string("\"") + n + "\"").c_str(),
      pub->get_topic_name());
    keep.push_back(pub);
  }
  // 不加 rt/ 前缀的"原生 DDS 话题"
  auto raw_qos = rclcpp::QoS(10).avoid_ros_namespace_conventions(true);
  auto raw_pub = planner->create_publisher<StringMsg>("raw_dds_topic", raw_qos);
  RCLCPP_INFO(log, "  %-28s -> %s （avoid_ros_namespace_conventions=true）",
    "\"raw_dds_topic\"", raw_pub->get_topic_name());

  auto srv = planner->create_service<ComputePath>("compute",
      [](const std::shared_ptr<ComputePath::Request>, std::shared_ptr<ComputePath::Response>) {});
  RCLCPP_INFO(log, "  服务 \"compute\"               -> %s", srv->get_service_name());

  auto action_srv = rclcpp_action::create_server<MoveTo>(planner, "move_to",
      [](const rclcpp_action::GoalUUID &, std::shared_ptr<const MoveTo::Goal>) {
        return rclcpp_action::GoalResponse::REJECT;
      },
      [](const std::shared_ptr<rclcpp_action::ServerGoalHandle<MoveTo>>) {
        return rclcpp_action::CancelResponse::REJECT;
      },
      [](const std::shared_ptr<rclcpp_action::ServerGoalHandle<MoveTo>>) {});

  // ---------- 2. 非法名字 ----------
  RCLCPP_INFO(log, "[2] 非法名字");
  for (const char * n : {"foo__bar", "~foo", "123abc", "foo/", "foo//bar", "{bad/sub}"}) {
    try_name(*planner, n);
  }
  // 长度：完整名字 /robot1/<base> 超过 247 个字符（255 减去为前缀保留的 8 个）
  try_name(*planner, std::string(239, 'a'));  // /robot1/ + 239 = 247，刚好合法
  try_name(*planner, std::string(240, 'a'));  // 248，超限

  // ---------- 3. ignore_local_publications ----------
  RCLCPP_INFO(log, "[3] ignore_local_publications=true 的订阅，谁的消息会被忽略？");
  rclcpp::SubscriptionOptions sub_opts;
  sub_opts.ignore_local_publications = true;
  int from_self = 0, from_sibling = 0, from_other_process = 0;
  auto sub = planner->create_subscription<StringMsg>("loop", 10,
      [&](StringMsg::ConstSharedPtr m) {
        if (m->data == "self") {
          ++from_self;
        } else if (m->data == "sibling") {
          ++from_sibling;
        } else {
          ++from_other_process;
        }
      }, sub_opts);
  // 对照组：controller 节点上一个普通订阅（不忽略本地发布）
  int control_count = 0;
  auto control_sub = controller->create_subscription<StringMsg>("loop", 10,
      [&](StringMsg::ConstSharedPtr) {++control_count;});
  auto self_pub = planner->create_publisher<StringMsg>("loop", 10);
  auto sibling_pub = controller->create_publisher<StringMsg>("loop", 10);

  // 一个执行器只能管同一个 context 的节点，所以 controller 用第二个执行器
  rclcpp::executors::SingleThreadedExecutor exec;
  rclcpp::ExecutorOptions eo2;
  if (ctx2) {eo2.context = ctx2;}
  rclcpp::executors::SingleThreadedExecutor exec2(eo2);
  exec.add_node(planner);
  exec2.add_node(controller);
  auto spin_both = [&](std::chrono::milliseconds d) {exec.spin_some(d); exec2.spin_some(d);};

  // 等发现完成再发
  auto t0 = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() - t0 < 1s) {spin_both(25ms);}
  for (int i = 0; i < 10; ++i) {
    StringMsg m;
    m.data = "self";
    self_pub->publish(m);
    m.data = "sibling";
    sibling_pub->publish(m);
    spin_both(10ms);
  }
  t0 = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() - t0 < 500ms) {spin_both(25ms);}
  RCLCPP_INFO(log, "  同节点发布 10 条，收到 %d；同进程另一个节点发布 10 条，收到 %d"
    "（对照组普通订阅共收到 %d 条）", from_self, from_sibling, control_count);

  if (hold_s > 0) {
    RCLCPP_INFO(log, "[4] 保持运行 %d 秒（另开终端：ros2 run design_lab participant_map，"
      "或者往 /robot1/loop 发消息）", hold_s);
    t0 = std::chrono::steady_clock::now();
    while (rclcpp::ok() && std::chrono::steady_clock::now() - t0 < std::chrono::seconds(hold_s)) {
      spin_both(50ms);
    }
    RCLCPP_INFO(log, "  期间收到其他进程发来的消息 %d 条", from_other_process);
  }
  if (ctx2) {ctx2->shutdown("done");}
  rclcpp::shutdown();
  return 0;
}
