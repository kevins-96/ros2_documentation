// 练习「进程内通信」：同一进程里发布和订阅时，消息到底有没有被复制？
// 运行：ros2 run perf_lab ipc_lab --case <名字>，见 labs/intra-process-zero-copy.md
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/int32.hpp"

using Msg = std_msgs::msg::Int32;
using namespace std::chrono_literals;

struct Case
{
  bool ipc = true;              // 节点是否开启 use_intra_process_comms
  bool publish_unique = true;   // publish(unique_ptr) 还是 publish(const T &)
  std::vector<std::string> subs;  // 每个订阅者的回调类型："unique" / "shared" / "ref"
  rclcpp::QoS qos = rclcpp::QoS(10);
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  const auto args = rclcpp::remove_ros_arguments(argc, argv);
  std::string name = "one-unique";
  for (size_t i = 1; i + 1 < args.size(); ++i) {
    if (args[i] == "--case") {name = args[i + 1];}
  }

  Case c;
  if (name == "one-unique") {
    c.subs = {"unique"};
  } else if (name == "two-unique") {
    c.subs = {"unique", "unique"};
  } else if (name == "two-shared") {
    c.subs = {"shared", "shared"};
  } else if (name == "shared-and-unique") {
    c.subs = {"shared", "unique"};
  } else if (name == "by-ref") {
    c.publish_unique = false;
    c.subs = {"unique"};
  } else if (name == "ipc-off") {
    c.ipc = false;
    c.subs = {"unique"};
  } else if (name == "keep-all") {
    c.subs = {"unique"};
    c.qos = rclcpp::QoS(rclcpp::KeepAll());
  } else {
    std::fprintf(stderr, "unknown case %s\n", name.c_str());
    return 1;
  }

  auto opts = rclcpp::NodeOptions().use_intra_process_comms(c.ipc);
  auto node = std::make_shared<rclcpp::Node>("ipc_lab", opts);

  std::vector<rclcpp::SubscriptionBase::SharedPtr> subs;
  try {
    for (size_t i = 0; i < c.subs.size(); ++i) {
      const auto kind = c.subs[i];
      auto print = [i, kind](const Msg & m) {
          std::printf("  sub%zu(%-6s) got %d at %p\n", i, kind.c_str(), m.data,
            static_cast<const void *>(&m));
        };
      if (kind == "unique") {
        subs.push_back(node->create_subscription<Msg>("n", c.qos,
          [print](Msg::UniquePtr m) {print(*m);}));
      } else if (kind == "shared") {
        subs.push_back(node->create_subscription<Msg>("n", c.qos,
          [print](Msg::ConstSharedPtr m) {print(*m);}));
      } else {
        subs.push_back(node->create_subscription<Msg>("n", c.qos,
          [print](const Msg & m) {print(m);}));
      }
    }
    auto pub = node->create_publisher<Msg>("n", c.qos);

    int count = 0;
    auto timer = node->create_wall_timer(200ms, [&]() {
        auto m = std::make_unique<Msg>();
        m->data = count++;
        std::printf("pub   %d at %p\n", m->data, static_cast<void *>(m.get()));
        if (c.publish_unique) {
          pub->publish(std::move(m));
        } else {
          pub->publish(*m);
        }
      });

    rclcpp::executors::SingleThreadedExecutor exec;
    exec.add_node(node);
    const auto end = std::chrono::steady_clock::now() + 1100ms;
    while (rclcpp::ok() && std::chrono::steady_clock::now() < end) {
      exec.spin_some(50ms);
    }
  } catch (const std::exception & e) {
    std::printf("exception: %s\n", e.what());
  }
  rclcpp::shutdown();
  return 0;
}
