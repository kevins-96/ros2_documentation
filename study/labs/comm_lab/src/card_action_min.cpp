// 卡片《动作》的最小示例：MoveTo 动作服务端（工作线程执行）+ 客户端（发目标、收反馈、取结果）
// 适用：ROS 2 Rolling（rclcpp / rclcpp_action）
#include <atomic>
#include <chrono>
#include <cmath>
#include <memory>
#include <thread>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include "study_interfaces/action/move_to.hpp"

using namespace std::chrono_literals;
using MoveTo = study_interfaces::action::MoveTo;
using ServerGoalHandle = rclcpp_action::ServerGoalHandle<MoveTo>;
using ClientGoalHandle = rclcpp_action::ClientGoalHandle<MoveTo>;

class MoveToServer : public rclcpp::Node
{
public:
  MoveToServer()
  : Node("move_to_server")
  {
    server_ = rclcpp_action::create_server<MoveTo>(
      this, "move_to",
      // 1. 收到目标：决定接受还是拒绝（必须很快返回）
      [this](const rclcpp_action::GoalUUID &, std::shared_ptr<const MoveTo::Goal> goal) {
        if (busy_) {                     // 一次只执行一个目标：忙时直接拒绝
          return rclcpp_action::GoalResponse::REJECT;
        }
        RCLCPP_INFO(get_logger(), "goal x=%.1f y=%.1f", goal->target.x, goal->target.y);
        return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
      },
      // 2. 收到取消请求：决定是否接受取消
      [](const std::shared_ptr<ServerGoalHandle>) {
        return rclcpp_action::CancelResponse::ACCEPT;
      },
      // 3. 目标已被接受：把长任务交给别的线程，这个回调必须立即返回
      [this](const std::shared_ptr<ServerGoalHandle> gh) {
        busy_ = true;
        if (worker_.joinable()) {worker_.join();}  // 上一个线程已结束（busy_ 为 false 才会走到这里）
        worker_ = std::thread([this, gh]() {execute(gh); busy_ = false;});
      });
  }

  ~MoveToServer() override
  {
    if (worker_.joinable()) {worker_.join();}
  }

private:
  void execute(const std::shared_ptr<ServerGoalHandle> gh)
  {
    auto feedback = std::make_shared<MoveTo::Feedback>();
    auto result = std::make_shared<MoveTo::Result>();
    float dist = std::hypot(gh->get_goal()->target.x, gh->get_goal()->target.y);
    rclcpp::Rate rate(10);
    while (rclcpp::ok() && dist > gh->get_goal()->tolerance_m) {
      if (gh->is_canceling()) {          // 客户端请求取消
        result->reached = false;
        result->final_error_m = dist;
        gh->canceled(result);
        return;
      }
      dist -= 0.1f;                      // 模拟靠近目标
      feedback->distance_remaining_m = dist;
      gh->publish_feedback(feedback);
      rate.sleep();
    }
    if (rclcpp::ok()) {
      result->reached = true;
      result->final_error_m = dist;
      gh->succeed(result);
    }
  }

  rclcpp_action::Server<MoveTo>::SharedPtr server_;
  std::thread worker_;
  std::atomic<bool> busy_{false};
};

class MoveToClient : public rclcpp::Node
{
public:
  MoveToClient()
  : Node("move_to_client")
  {
    client_ = rclcpp_action::create_client<MoveTo>(this, "move_to");
    timer_ = create_wall_timer(500ms, [this]() {send_once();});
  }

private:
  void send_once()
  {
    if (!client_->action_server_is_ready()) {return;}
    timer_->cancel();

    MoveTo::Goal goal;
    goal.target.x = 1.0;
    goal.tolerance_m = 0.1f;

    rclcpp_action::Client<MoveTo>::SendGoalOptions opts;
    opts.goal_response_callback = [this](const ClientGoalHandle::SharedPtr & gh) {
        RCLCPP_INFO(get_logger(), gh ? "goal accepted" : "goal rejected");
      };
    opts.feedback_callback =
      [this](ClientGoalHandle::SharedPtr, const std::shared_ptr<const MoveTo::Feedback> fb) {
        RCLCPP_INFO(get_logger(), "remaining %.2f m", fb->distance_remaining_m);
      };
    opts.result_callback = [this](const ClientGoalHandle::WrappedResult & wr) {
        RCLCPP_INFO(
          get_logger(), "result code=%d reached=%d", static_cast<int>(wr.code),
          wr.result->reached);
        rclcpp::shutdown();
      };
    client_->async_send_goal(goal, opts);
  }

  rclcpp_action::Client<MoveTo>::SharedPtr client_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::executors::SingleThreadedExecutor exec;
  auto server = std::make_shared<MoveToServer>();
  auto client = std::make_shared<MoveToClient>();
  exec.add_node(server);
  exec.add_node(client);
  exec.spin();
  return 0;
}
