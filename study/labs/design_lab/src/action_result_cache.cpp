// 练习：动作的"结果缓存"和"状态话题"是怎么设计的？
//
// 设计文档：Actions（Get Result Service / Result caching / Goal Status Topic）
//   1. 列出一个动作在 ROS 图里展开成的隐藏话题和服务
//   2. 目标完成后，另一个"旁观者"直接调用隐藏的 get_result 服务，3 s 后和 12 s 后各取一次
//   3. 目标完成后才创建的状态订阅（TransientLocal）能不能拿到最后的状态
//
// 用法：ros2 run design_lab action_result_cache [--result-timeout 秒]
//   不给 --result-timeout 时用 rcl_action 的默认值
// 适用：ROS 2 Rolling（rclcpp_action / rcl_action 10.x）
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <thread>

#include "action_msgs/msg/goal_status_array.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include "study_interfaces/action/move_to.hpp"

using namespace std::chrono_literals;
using MoveTo = study_interfaces::action::MoveTo;
using GetResult = MoveTo::Impl::GetResultService;  // 动作内部的"取结果"服务类型
using GoalStatus = action_msgs::msg::GoalStatus;

namespace
{
const char * status_name(int8_t s)
{
  switch (s) {
    case GoalStatus::STATUS_UNKNOWN: return "UNKNOWN";
    case GoalStatus::STATUS_ACCEPTED: return "ACCEPTED";
    case GoalStatus::STATUS_EXECUTING: return "EXECUTING";
    case GoalStatus::STATUS_CANCELING: return "CANCELING";
    case GoalStatus::STATUS_SUCCEEDED: return "SUCCEEDED";
    case GoalStatus::STATUS_CANCELED: return "CANCELED";
    case GoalStatus::STATUS_ABORTED: return "ABORTED";
    default: return "?";
  }
}
}  // namespace

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto server_opts = rcl_action_server_get_default_options();
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::strcmp(argv[i], "--result-timeout") == 0) {
      server_opts.result_timeout.nanoseconds = RCL_S_TO_NS(std::atoll(argv[i + 1]));
    }
  }
  std::printf("result_timeout = %.1f s\n", server_opts.result_timeout.nanoseconds / 1e9);

  auto server_node = std::make_shared<rclcpp::Node>("move_to_server");
  auto client_node = std::make_shared<rclcpp::Node>("task_manager");
  auto observer = std::make_shared<rclcpp::Node>("observer");

  auto server = rclcpp_action::create_server<MoveTo>(server_node, "move_to",
      [](const rclcpp_action::GoalUUID &, std::shared_ptr<const MoveTo::Goal>) {
        return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
      },
      [](const std::shared_ptr<rclcpp_action::ServerGoalHandle<MoveTo>>) {
        return rclcpp_action::CancelResponse::ACCEPT;
      },
      [](const std::shared_ptr<rclcpp_action::ServerGoalHandle<MoveTo>> gh) {
        std::thread([gh]() {
          std::this_thread::sleep_for(200ms);
          auto r = std::make_shared<MoveTo::Result>();
          r->reached = true;
          gh->succeed(r);
        }).detach();
      },
      server_opts);

  auto client = rclcpp_action::create_client<MoveTo>(client_node, "move_to");
  auto raw_get_result = observer->create_client<GetResult>("move_to/_action/get_result");

  rclcpp::executors::MultiThreadedExecutor exec;
  exec.add_node(server_node);
  exec.add_node(client_node);
  exec.add_node(observer);
  std::thread spin([&]() {exec.spin();});

  client->wait_for_action_server(5s);
  std::this_thread::sleep_for(500ms);

  std::printf("[1] 一个动作在图里展开成：\n");
  for (const auto & [name, types] : observer->get_topic_names_and_types()) {
    if (name.find("move_to") != std::string::npos) {
      std::printf("    话题 %-36s %s\n", name.c_str(), types[0].c_str());
    }
  }
  for (const auto & [name, types] : observer->get_service_names_and_types()) {
    if (name.find("move_to/_action") != std::string::npos) {
      std::printf("    服务 %-36s %s\n", name.c_str(), types[0].c_str());
    }
  }

  // 正常流程：发目标、等结果
  MoveTo::Goal goal;
  goal.target.x = 1.0;
  auto gh = client->async_send_goal(goal).get();
  auto wrapped = client->async_get_result(gh).get();
  auto goal_id = gh->get_goal_id();
  auto t_done = std::chrono::steady_clock::now();
  std::printf("[2] 目标完成：%s\n", wrapped.code == rclcpp_action::ResultCode::SUCCEEDED ?
    "SUCCEEDED" : "其他");

  // 目标完成后才创建的状态订阅
  auto status_qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local();
  auto status_sub = observer->create_subscription<action_msgs::msg::GoalStatusArray>(
    "move_to/_action/status", status_qos,
    [](action_msgs::msg::GoalStatusArray::ConstSharedPtr m) {
      std::printf("[3] 后加入的状态订阅收到 %zu 个目标的状态%s%s\n", m->status_list.size(),
      m->status_list.empty() ? "" : "，第一个是 ",
      m->status_list.empty() ? "" : status_name(m->status_list[0].status));
    });

  // 旁观者直接调用隐藏的 get_result 服务
  for (auto wait : {3s, 12s}) {
    std::this_thread::sleep_until(t_done + wait);
    auto req = std::make_shared<GetResult::Request>();
    req->goal_id.uuid = goal_id;
    auto fut = raw_get_result->async_send_request(req);
    if (fut.wait_for(2s) == std::future_status::ready) {
      auto res = fut.get();
      std::printf("[2] 完成后 %2ld s，旁观者调用 get_result：status=%s reached=%s\n",
        static_cast<long>(wait.count()), status_name(res->status),
        res->result.reached ? "true" : "false");
    } else {
      std::printf("[2] 完成后 %2ld s，旁观者调用 get_result：2 s 内没有响应\n",
        static_cast<long>(wait.count()));
    }
  }

  // 结果过期之后再建一个状态订阅，看状态列表里还有没有这个目标
  auto status_sub2 = observer->create_subscription<action_msgs::msg::GoalStatusArray>(
    "move_to/_action/status", status_qos,
    [](action_msgs::msg::GoalStatusArray::ConstSharedPtr m) {
      std::printf("[3] 结果过期后新建的状态订阅收到 %zu 个目标的状态%s%s\n", m->status_list.size(),
      m->status_list.empty() ? "" : "，第一个是 ",
      m->status_list.empty() ? "" : status_name(m->status_list[0].status));
    });
  std::this_thread::sleep_for(1s);

  exec.cancel();
  spin.join();
  rclcpp::shutdown();
  return 0;
}
