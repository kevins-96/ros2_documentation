// 实验 B：在定时器回调里"同步"等待服务结果，什么时候会死锁？
//
// 改写自仓库文档 Using-callback-groups.rst 的示例，做了两处调整：
//   1. 服务端和客户端放在同一个进程、同一个 MultiThreadedExecutor 里，一条命令就能跑；
//   2. create_client 改用 rclcpp::ServicesQoS()（Rolling 的 rclcpp 已没有接受
//      rmw_qos_profile_t 的 create_client 重载，文档原示例在 Rolling 上编译不过）。
//
// 命令行参数：
//   --groups same       定时器和 client 都在节点默认组（同一个互斥组）  -> 预期超时/死锁
//   --groups separate   定时器和 client 各自一个互斥组                    -> 预期正常
//   --groups reentrant  定时器和 client 共用一个可重入组                  -> 预期正常
//
// 适用：ROS 2 Rolling（rclcpp）

#include <chrono>
#include <cstring>
#include <future>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "std_srvs/srv/empty.hpp"

using namespace std::chrono_literals;

class ServerNode : public rclcpp::Node
{
public:
  ServerNode()
  : Node("service_node")
  {
    srv_ = create_service<std_srvs::srv::Empty>(
      "test_service",
      [this](
        const std::shared_ptr<std_srvs::srv::Empty::Request>,
        std::shared_ptr<std_srvs::srv::Empty::Response>)
      {
        RCLCPP_INFO(get_logger(), "Received request, responding...");
      });
  }

private:
  rclcpp::Service<std_srvs::srv::Empty>::SharedPtr srv_;
};

class ClientNode : public rclcpp::Node
{
public:
  explicit ClientNode(const std::string & mode)
  : Node("client_node")
  {
    if (mode == "separate") {
      client_cb_group_ = create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);
      timer_cb_group_ = create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);
    } else if (mode == "reentrant") {
      client_cb_group_ = create_callback_group(rclcpp::CallbackGroupType::Reentrant);
      timer_cb_group_ = client_cb_group_;
    } else {
      client_cb_group_ = nullptr;  // 默认组
      timer_cb_group_ = nullptr;   // 同一个默认组
    }

    client_ = create_client<std_srvs::srv::Empty>(
      "test_service", rclcpp::ServicesQoS(), client_cb_group_);

    timer_ = create_wall_timer(
      1s, [this]() {
        RCLCPP_INFO(get_logger(), "Sending request");
        auto future = client_->async_send_request(
          std::make_shared<std_srvs::srv::Empty::Request>());
        // 用 wait_for 模拟"同步调用"；加超时是为了让程序还能继续打印、可以 Ctrl-C 退出
        if (future.wait_for(3s) == std::future_status::ready) {
          RCLCPP_INFO(get_logger(), "Received response");
        } else {
          RCLCPP_WARN(get_logger(), "TIMEOUT: response callback never ran (deadlock pattern)");
          client_->remove_pending_request(future);
        }
      }, timer_cb_group_);
  }

private:
  rclcpp::CallbackGroup::SharedPtr client_cb_group_;
  rclcpp::CallbackGroup::SharedPtr timer_cb_group_;
  rclcpp::Client<std_srvs::srv::Empty>::SharedPtr client_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);

  std::string mode = "same";
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::strcmp(argv[i], "--groups") == 0) {mode = argv[i + 1];}
  }

  auto server = std::make_shared<ServerNode>();
  auto client = std::make_shared<ClientNode>(mode);
  RCLCPP_INFO(client->get_logger(), "groups=%s", mode.c_str());

  rclcpp::executors::MultiThreadedExecutor exec;
  exec.add_node(server);
  exec.add_node(client);
  exec.spin();

  rclcpp::shutdown();
  return 0;
}
