// 第 11 批：在同一个进程（同一个 context）里建几个节点，逐个尝试创建发布者/订阅者，
// 看 SROS2 的访问控制到底按"节点"还是按"enclave"生效。
//
// 用法：sec_probe <节点名>:<话题>[:pub|sub] ...
//   例：sec_probe a:ta b:tb a:tb:sub
//   第一个参数写成 --second-context=/某个enclave 时，再建一个 context（用那个 enclave），
//   在里面建节点 z 发布 other：看一个进程能不能同时用两个 enclave。
// 同名节点只建一次；每个条目单独 try/catch，失败不会让进程退出。
#include <chrono>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

using namespace std::chrono_literals;

struct Entry
{
  std::string node, topic, kind;
};

static Entry parse(const std::string & s)
{
  Entry e;
  auto p1 = s.find(':');
  e.node = s.substr(0, p1);
  auto rest = s.substr(p1 + 1);
  auto p2 = rest.find(':');
  e.topic = rest.substr(0, p2);
  e.kind = (p2 == std::string::npos) ? "pub" : rest.substr(p2 + 1);
  return e;
}

int main(int argc, char ** argv)
{
  auto args = rclcpp::init_and_remove_ros_arguments(argc, argv);
  std::vector<Entry> entries;
  std::string second_enclave;
  for (size_t i = 1; i < args.size(); ++i) {
    if (args[i].rfind("--second-context=", 0) == 0) {
      second_enclave = args[i].substr(17);
    } else {
      entries.push_back(parse(args[i]));
    }
  }

  rclcpp::Context::SharedPtr ctx2;
  rclcpp::Node::SharedPtr z;
  rclcpp::PublisherBase::SharedPtr z_pub;
  if (!second_enclave.empty()) {
    ctx2 = std::make_shared<rclcpp::Context>();
    const char * argv2[] = {"sec_probe", "--ros-args", "-e", second_enclave.c_str()};
    ctx2->init(4, argv2);
    rclcpp::NodeOptions opt;
    opt.context(ctx2);
    try {
      z = std::make_shared<rclcpp::Node>("z", opt);
      z_pub = z->create_publisher<std_msgs::msg::String>("other", 10);
      std::printf("ctx2   z      other (enclave %s)  OK\n", second_enclave.c_str());
    } catch (const std::exception & ex) {
      std::printf("ctx2   z      FAIL: %.90s\n", ex.what());
    }
  }

  std::map<std::string, rclcpp::Node::SharedPtr> nodes;
  std::vector<rclcpp::PublisherBase::SharedPtr> pubs;
  std::vector<rclcpp::SubscriptionBase::SharedPtr> subs;
  std::map<std::string, int> received;

  for (const auto & e : entries) {
    if (!nodes.count(e.node)) {
      try {
        nodes[e.node] = std::make_shared<rclcpp::Node>(e.node);
        std::printf("node   %-6s OK\n", e.node.c_str());
      } catch (const std::exception & ex) {
        std::printf("node   %-6s FAIL: %s\n", e.node.c_str(), ex.what());
        nodes[e.node] = nullptr;
      }
    }
    auto node = nodes[e.node];
    if (!node) {continue;}
    try {
      if (e.kind == "pub") {
        pubs.push_back(node->create_publisher<std_msgs::msg::String>(e.topic, 10));
      } else {
        auto key = e.node + "<-" + e.topic;
        received[key] = 0;
        subs.push_back(node->create_subscription<std_msgs::msg::String>(
            e.topic, 10, [&received, key](std_msgs::msg::String::ConstSharedPtr) {++received[key];}));
      }
      std::printf("%-6s %-6s %-24s OK\n", e.kind.c_str(), e.node.c_str(), e.topic.c_str());
    } catch (const std::exception & ex) {
      std::string msg = ex.what();
      std::printf("%-6s %-6s %-24s FAIL: %s\n", e.kind.c_str(), e.node.c_str(), e.topic.c_str(),
        msg.substr(0, 90).c_str());
    }
  }

  // 让发布者发 2 秒，看订阅者收不收得到
  rclcpp::executors::SingleThreadedExecutor exec;
  for (auto & [name, n] : nodes) {if (n) {exec.add_node(n);}}
  auto start = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() - start < 2s) {
    for (auto & p : pubs) {
      auto typed = std::static_pointer_cast<rclcpp::Publisher<std_msgs::msg::String>>(p);
      std_msgs::msg::String m;
      m.data = "x";
      typed->publish(m);
    }
    exec.spin_some(100ms);
    std::this_thread::sleep_for(100ms);
  }
  for (auto & [k, v] : received) {std::printf("recv   %-24s %d\n", k.c_str(), v);}
  if (ctx2) {z_pub.reset(); z.reset(); ctx2->shutdown("done");}
  rclcpp::shutdown();
  return 0;
}
