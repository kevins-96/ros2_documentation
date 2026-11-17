// 练习：动作服务端忙的时候来了新目标，怎么办？
//
//   --policy reject   忙时拒绝新目标（旧目标继续执行）
//   --policy preempt  接受新目标，并中止（abort）正在执行的旧目标
//
// 客户端在 t≈0.5 s 发送目标 A（需要约 2 s 完成），t≈1.5 s 发送目标 B。
// 适用：ROS 2 Rolling（rclcpp / rclcpp_action）
#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include "study_interfaces/action/move_to.hpp"

using namespace std::chrono_literals;
using MoveTo = study_interfaces::action::MoveTo;
using ServerGoalHandle = rclcpp_action::ServerGoalHandle<MoveTo>;
using ClientGoalHandle = rclcpp_action::ClientGoalHandle<MoveTo>;

class Server : public rclcpp::Node
{
public:
  explicit Server(bool preempt)
  : Node("move_to_server"), preempt_(preempt)
  {
    server_ = rclcpp_action::create_server<MoveTo>(
      this, "move_to",
      [this](const rclcpp_action::GoalUUID &, std::shared_ptr<const MoveTo::Goal> goal) {
        std::lock_guard<std::mutex> lk(mtx_);
        if (active_ && !preempt_) {
          RCLCPP_WARN(get_logger(), "busy -> REJECT goal x=%.1f", goal->target.x);
          return rclcpp_action::GoalResponse::REJECT;
        }
        return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
      },
      [](const std::shared_ptr<ServerGoalHandle>) {return rclcpp_action::CancelResponse::ACCEPT;},
      [this](const std::shared_ptr<ServerGoalHandle> gh) {
        {
          std::lock_guard<std::mutex> lk(mtx_);
          if (active_) {
            RCLCPP_WARN(get_logger(), "new goal x=%.1f preempts the active goal",
            gh->get_goal()->target.x);
          }
          active_ = gh;  // 旧目标的执行线程会发现自己不再是 active_，然后 abort
        }
        workers_.emplace_back([this, gh]() {execute(gh);});
      });
  }

  ~Server() override
  {
    for (auto & t : workers_) {if (t.joinable()) {t.join();}}
  }

private:
  void execute(const std::shared_ptr<ServerGoalHandle> gh)
  {
    auto result = std::make_shared<MoveTo::Result>();
    auto feedback = std::make_shared<MoveTo::Feedback>();
    float dist = 2.0f;
    rclcpp::Rate rate(10);
    while (rclcpp::ok() && dist > 0.0f) {
      {
        std::lock_guard<std::mutex> lk(mtx_);
        if (active_ != gh) {           // 被新目标抢占
          result->final_error_m = dist;
          gh->abort(result);
          return;
        }
      }
      dist -= 0.1f;
      feedback->distance_remaining_m = dist;
      gh->publish_feedback(feedback);
      rate.sleep();
    }
    std::lock_guard<std::mutex> lk(mtx_);
    if (rclcpp::ok() && active_ == gh) {
      result->reached = true;
      gh->succeed(result);
      active_.reset();
    }
  }

  bool preempt_;
  std::mutex mtx_;
  std::shared_ptr<ServerGoalHandle> active_;
  std::vector<std::thread> workers_;
  rclcpp_action::Server<MoveTo>::SharedPtr server_;
};

class Client : public rclcpp::Node
{
public:
  Client()
  : Node("move_to_client")
  {
    client_ = rclcpp_action::create_client<MoveTo>(this, "move_to");
    start_ = now();
    timer_ = create_wall_timer(100ms, [this]() {tick();});
  }

private:
  void tick()
  {
    if (!client_->action_server_is_ready()) {return;}
    const double t = (now() - start_).seconds();
    if (!sent_a_ && t > 0.5) {send("A", 1.0); sent_a_ = true;}
    if (!sent_b_ && t > 1.5) {send("B", 2.0); sent_b_ = true;}
    if (done_ >= 2 || t > 6.0) {rclcpp::shutdown();}
  }

  void send(const std::string & name, double x)
  {
    MoveTo::Goal goal;
    goal.target.x = x;
    rclcpp_action::Client<MoveTo>::SendGoalOptions opts;
    opts.goal_response_callback = [this, name](const ClientGoalHandle::SharedPtr & gh) {
        RCLCPP_INFO(get_logger(), "[%s] %s", name.c_str(), gh ? "accepted" : "REJECTED");
        if (!gh) {++done_;}
      };
    opts.result_callback = [this, name](const ClientGoalHandle::WrappedResult & wr) {
        const char * code =
          wr.code == rclcpp_action::ResultCode::SUCCEEDED ? "SUCCEEDED" :
          wr.code == rclcpp_action::ResultCode::ABORTED ? "ABORTED" :
          wr.code == rclcpp_action::ResultCode::CANCELED ? "CANCELED" : "UNKNOWN";
        RCLCPP_INFO(get_logger(), "[%s] result %s", name.c_str(), code);
        ++done_;
      };
    RCLCPP_INFO(get_logger(), "[%s] send goal x=%.1f", name.c_str(), x);
    client_->async_send_goal(goal, opts);
  }

  rclcpp_action::Client<MoveTo>::SharedPtr client_;
  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::Time start_;
  bool sent_a_{false};
  bool sent_b_{false};
  int done_{0};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  bool preempt = false;
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::strcmp(argv[i], "--policy") == 0) {preempt = std::strcmp(argv[i + 1], "preempt") == 0;}
  }
  rclcpp::executors::SingleThreadedExecutor exec;
  auto server = std::make_shared<Server>(preempt);
  auto client = std::make_shared<Client>();
  exec.add_node(server);
  exec.add_node(client);
  exec.spin();
  return 0;
}
