// 卡片《服务》的最小示例：规划服务端 + 用回调取结果的客户端（不阻塞执行器）
// 适用：ROS 2 Rolling（rclcpp）
#include <chrono>
#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "study_interfaces/srv/compute_path.hpp"

using namespace std::chrono_literals;
using ComputePath = study_interfaces::srv::ComputePath;

class Planner : public rclcpp::Node
{
public:
  Planner()
  : Node("planner")
  {
    srv_ = create_service<ComputePath>(
      "compute_path",
      [](const std::shared_ptr<ComputePath::Request> req,
      std::shared_ptr<ComputePath::Response> res) {
        // 服务应当很快返回；耗时的规划请改用 action
        res->path = {req->start, req->goal};
        res->success = true;
      });
  }

private:
  rclcpp::Service<ComputePath>::SharedPtr srv_;
};

class TaskManager : public rclcpp::Node
{
public:
  TaskManager()
  : Node("task_manager")
  {
    client_ = create_client<ComputePath>("compute_path");
    timer_ = create_wall_timer(1s, [this]() {request_path();});
  }

private:
  void request_path()
  {
    if (!client_->service_is_ready()) {
      RCLCPP_WARN(get_logger(), "compute_path not available yet");
      return;
    }
    auto req = std::make_shared<ComputePath::Request>();
    req->goal.x = 3.0;
    // 带回调的 async_send_request：结果到达时由执行器调用回调，定时器回调立即返回
    client_->async_send_request(
      req, [this](rclcpp::Client<ComputePath>::SharedFuture future) {
        auto res = future.get();
        RCLCPP_INFO(get_logger(), "path with %zu poses, success=%d",
        res->path.size(), res->success);
      });
  }

  rclcpp::Client<ComputePath>::SharedPtr client_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::executors::SingleThreadedExecutor exec;  // 单线程也不会死锁，因为没有阻塞等待
  auto planner = std::make_shared<Planner>();
  auto task = std::make_shared<TaskManager>();
  exec.add_node(planner);
  exec.add_node(task);
  exec.spin();
  rclcpp::shutdown();
  return 0;
}
