// 练习：进程内通信（intra-process）和借用消息（loaned message）的设计说法，在 Rolling 上还成立吗？
//
// 设计文档：Intra-process Communications in ROS 2（2020-03）、Zero Copy via Loaned Messages（2020-02）
//
//   --case per-sub-buffer   每个订阅者一个缓冲区：深度 2 和深度 5 的订阅者，先发 8 条再 spin
//   --case keep-all         KeepAll 的订阅者 / 发布者能不能开进程内通信
//   --case transient-local  先发 5 条，再建 transient_local / volatile 的订阅者，晚到的能收到几条
//   --case reliability      best_effort 发布者 + reliable 订阅者（不兼容）在进程内还通不通
//   --case buffer-type      两个 UniquePtr 订阅者，缓冲区类型 CallbackDefault vs SharedPtr，谁拿到原件
//   --case double-queue     开放问题：同一订阅者同时收进程内和进程间消息，深度 3 最多能攒几条
//   --case topics           开了进程内通信后，图里还有没有 /xxx/_intra 话题
//   --case mixed-transient  transient_local 发布者只有进程内订阅者时，晚到的进程间订阅者能收到历史吗
//   --case loan             Float64、FixedBlock、String 能不能借用；借用消息遇到进程内订阅者会怎样
//
// 用法：ros2 run design_lab ipc_design_probe --case per-sub-buffer
// 适用：ROS 2 Rolling（rclcpp 33.x，rmw_fastrtps_cpp）
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/float64.hpp"
#include "std_msgs/msg/int32.hpp"
#include "std_msgs/msg/string.hpp"
#include "study_interfaces/msg/fixed_block.hpp"

using Int32 = std_msgs::msg::Int32;
using namespace std::chrono_literals;

static rclcpp::Node::SharedPtr make_node(const std::string & name, bool ipc)
{
  return std::make_shared<rclcpp::Node>(name, rclcpp::NodeOptions().use_intra_process_comms(ipc));
}

// 反复 spin_some，直到把已经到达的消息都处理完
static void drain(rclcpp::Executor & exec, int rounds = 20)
{
  for (int i = 0; i < rounds; ++i) {
    exec.spin_some();
    rclcpp::sleep_for(10ms);
  }
}

static std::string join(const std::vector<int> & v)
{
  std::string s = "[";
  for (size_t i = 0; i < v.size(); ++i) {s += (i ? "," : "") + std::to_string(v[i]);}
  return s + "]";
}

static void per_sub_buffer(bool ipc)
{
  auto node = make_node("ipc_probe", ipc);
  std::vector<int> a, b;
  auto pub = node->create_publisher<Int32>("buf", rclcpp::QoS(10));
  auto sa = node->create_subscription<Int32>("buf", rclcpp::QoS(2),
      [&a](Int32::ConstSharedPtr m) {a.push_back(m->data);});
  auto sb = node->create_subscription<Int32>("buf", rclcpp::QoS(5),
      [&b](Int32::ConstSharedPtr m) {b.push_back(m->data);});
  rclcpp::executors::SingleThreadedExecutor exec;
  exec.add_node(node);
  rclcpp::sleep_for(300ms);
  for (int i = 0; i < 8; ++i) {
    auto m = std::make_unique<Int32>();
    m->data = i;
    pub->publish(std::move(m));
  }
  rclcpp::sleep_for(200ms);
  drain(exec);
  std::printf("ipc=%d 发布者深度 10，先发 0..7 再 spin：深度 2 的订阅者收到 %s，深度 5 的收到 %s\n",
    int(ipc), join(a).c_str(), join(b).c_str());
}

static void keep_all()
{
  auto node = make_node("ipc_probe", true);
  try {
    auto s = node->create_subscription<Int32>("ka", rclcpp::QoS(rclcpp::KeepAll()),
        [](Int32::ConstSharedPtr) {});
    std::printf("KeepAll 订阅者 + 进程内通信：创建成功\n");
  } catch (const std::exception & e) {
    std::printf("KeepAll 订阅者 + 进程内通信：抛异常 %s\n", e.what());
  }
  try {
    auto p = node->create_publisher<Int32>("ka2", rclcpp::QoS(rclcpp::KeepAll()));
    std::printf("KeepAll 发布者 + 进程内通信：创建成功\n");
  } catch (const std::exception & e) {
    std::printf("KeepAll 发布者 + 进程内通信：抛异常 %s\n", e.what());
  }
}

static void transient_local(bool ipc)
{
  auto node = make_node("ipc_probe", ipc);
  auto pub = node->create_publisher<Int32>("tl", rclcpp::QoS(3).transient_local());
  for (int i = 0; i < 5; ++i) {
    auto m = std::make_unique<Int32>();
    m->data = i;
    pub->publish(std::move(m));
  }
  std::vector<int> late_tl, late_vol;
  auto s1 = node->create_subscription<Int32>("tl", rclcpp::QoS(10).transient_local(),
      [&late_tl](Int32::ConstSharedPtr m) {late_tl.push_back(m->data);});
  auto s2 = node->create_subscription<Int32>("tl", rclcpp::QoS(10),
      [&late_vol](Int32::ConstSharedPtr m) {late_vol.push_back(m->data);});
  rclcpp::executors::SingleThreadedExecutor exec;
  exec.add_node(node);
  rclcpp::sleep_for(300ms);
  drain(exec);
  std::printf("ipc=%d 发布者 transient_local 深度 3，先发 0..4：晚到的 transient_local 订阅者收到 %s，"
    "volatile 订阅者收到 %s\n", int(ipc), join(late_tl).c_str(), join(late_vol).c_str());
}

static void reliability(bool ipc)
{
  auto node = make_node("ipc_probe", ipc);
  int be_to_rel = 0, rel_to_be = 0;
  auto p1 = node->create_publisher<Int32>("r1", rclcpp::QoS(10).best_effort());
  auto s1 = node->create_subscription<Int32>("r1", rclcpp::QoS(10).reliable(),
      [&be_to_rel](Int32::ConstSharedPtr) {++be_to_rel;});
  auto p2 = node->create_publisher<Int32>("r2", rclcpp::QoS(10).reliable());
  auto s2 = node->create_subscription<Int32>("r2", rclcpp::QoS(10).best_effort(),
      [&rel_to_be](Int32::ConstSharedPtr) {++rel_to_be;});
  rclcpp::executors::SingleThreadedExecutor exec;
  exec.add_node(node);
  rclcpp::sleep_for(300ms);
  for (int i = 0; i < 5; ++i) {
    p1->publish(std::make_unique<Int32>());
    p2->publish(std::make_unique<Int32>());
  }
  rclcpp::sleep_for(100ms);
  drain(exec);
  std::printf("ipc=%d best_effort 发布 -> reliable 订阅：收到 %d/5；reliable 发布 -> best_effort 订阅："
    "收到 %d/5\n", int(ipc), be_to_rel, rel_to_be);
}

static void buffer_type()
{
  for (auto bt : {rclcpp::IntraProcessBufferType::CallbackDefault,
      rclcpp::IntraProcessBufferType::SharedPtr})
  {
    const char * name = bt == rclcpp::IntraProcessBufferType::SharedPtr ? "SharedPtr" :
      "CallbackDefault";
    auto node = make_node("ipc_probe", true);
    rclcpp::SubscriptionOptions so;
    so.intra_process_buffer_type = bt;
    const void * got[2] = {nullptr, nullptr};
    auto pub = node->create_publisher<Int32>("bt", 10);
    auto s0 = node->create_subscription<Int32>("bt", 10,
        [&got](Int32::UniquePtr m) {got[0] = m.get();}, so);
    auto s1 = node->create_subscription<Int32>("bt", 10,
        [&got](Int32::UniquePtr m) {got[1] = m.get();}, so);
    rclcpp::executors::SingleThreadedExecutor exec;
    exec.add_node(node);
    auto m = std::make_unique<Int32>();
    const void * sent = m.get();
    pub->publish(std::move(m));
    drain(exec, 5);
    std::printf("缓冲区 %-15s 发布 %p，订阅者 1 收到 %p（%s），订阅者 2 收到 %p（%s）\n", name, sent,
      got[0], got[0] == sent ? "原件" : "副本", got[1], got[1] == sent ? "原件" : "副本");
  }
}

static void double_queue()
{
  // 订阅者所在节点开进程内通信；同进程另一个节点关掉进程内通信，它的消息只能走 DDS
  auto node = make_node("ipc_probe", true);
  auto other = make_node("ipc_probe_dds", false);
  std::vector<int> got;
  auto sub = node->create_subscription<Int32>("dq", rclcpp::QoS(3),
      [&got](Int32::ConstSharedPtr m) {got.push_back(m->data);});
  auto pub_intra = node->create_publisher<Int32>("dq", rclcpp::QoS(10));
  auto pub_dds = other->create_publisher<Int32>("dq", rclcpp::QoS(10));
  rclcpp::executors::SingleThreadedExecutor exec;
  exec.add_node(node);
  exec.add_node(other);
  rclcpp::sleep_for(500ms);
  for (int i = 0; i < 5; ++i) {
    auto a = std::make_unique<Int32>();
    a->data = 100 + i;
    pub_intra->publish(std::move(a));
    Int32 b;
    b.data = 200 + i;
    pub_dds->publish(b);
  }
  rclcpp::sleep_for(300ms);
  drain(exec);
  std::printf("订阅者深度 3；进程内发 100..104、进程间发 200..204 后才 spin：收到 %zu 条 %s\n",
    got.size(), join(got).c_str());
}

static void topics()
{
  auto node = make_node("ipc_probe", true);
  auto pub = node->create_publisher<Int32>("scan_like", 10);
  auto sub = node->create_subscription<Int32>("scan_like", 10, [](Int32::ConstSharedPtr) {});
  rclcpp::sleep_for(500ms);
  std::printf("开了进程内通信后，图里的话题：\n");
  for (const auto & [name, types] : node->get_topic_names_and_types()) {
    std::printf("  %s\n", name.c_str());
  }
}

static void mixed_transient()
{
  auto node = make_node("ipc_probe", true);
  std::vector<int> intra_got, late_dds_got;
  auto pub = node->create_publisher<Int32>("mt", rclcpp::QoS(3).transient_local());
  auto intra_sub = node->create_subscription<Int32>("mt", rclcpp::QoS(10),
      [&intra_got](Int32::ConstSharedPtr m) {intra_got.push_back(m->data);});
  rclcpp::executors::SingleThreadedExecutor exec;
  exec.add_node(node);
  rclcpp::sleep_for(300ms);
  for (int i = 0; i < 5; ++i) {
    auto m = std::make_unique<Int32>();
    m->data = i;
    pub->publish(std::move(m));
  }
  drain(exec, 5);
  // 发完以后才出现一个只能走 DDS 的订阅者
  auto other = make_node("ipc_probe_late", false);
  auto late = other->create_subscription<Int32>("mt", rclcpp::QoS(10).transient_local(),
      [&late_dds_got](Int32::ConstSharedPtr m) {late_dds_got.push_back(m->data);});
  exec.add_node(other);
  rclcpp::sleep_for(500ms);
  drain(exec);
  std::printf("进程内订阅者收到 %s；发完之后才出现的进程间 transient_local 订阅者收到 %s\n",
    join(intra_got).c_str(), join(late_dds_got).c_str());
}

template<typename MsgT>
static void print_loan(const char * name)
{
  auto node = make_node("ipc_probe_loan", false);
  auto pub = node->create_publisher<MsgT>(std::string("loan_") + name, 10);
  auto sub = node->create_subscription<MsgT>(std::string("loan_") + name, 10,
      [](typename MsgT::ConstSharedPtr) {});
  std::printf("%-11s 发布者 can_loan_messages=%d，订阅者 can_loan_messages=%d\n", name,
    int(pub->can_loan_messages()), int(sub->can_loan_messages()));
}

static void loan()
{
  using Block = study_interfaces::msg::FixedBlock;
  print_loan<std_msgs::msg::Float64>("Float64");
  print_loan<Block>("FixedBlock");
  print_loan<std_msgs::msg::String>("String");

  for (bool ipc : {false, true}) {
    auto node = make_node("ipc_probe_loan2", ipc);
    int count = 0;
    const void * got = nullptr;
    auto pub = node->create_publisher<Block>("loan_pub", 10);
    auto sub = node->create_subscription<Block>("loan_pub", 10,
        [&](Block::ConstSharedPtr m) {++count; got = m.get();});
    rclcpp::executors::SingleThreadedExecutor exec;
    exec.add_node(node);
    rclcpp::sleep_for(300ms);
    auto loaned = pub->borrow_loaned_message();
    loaned.get().seq = 42;
    const void * sent = &loaned.get();
    pub->publish(std::move(loaned));
    rclcpp::sleep_for(200ms);
    drain(exec);
    std::printf("ipc=%d 发布借用的 FixedBlock（地址 %p）：同进程订阅者收到 %d 条，地址 %p\n",
      int(ipc), sent, count, got);
  }
}

int main(int argc, char ** argv)
{
  auto args = rclcpp::init_and_remove_ros_arguments(argc, argv);
  std::string which = "per-sub-buffer";
  bool ipc = true;
  for (size_t i = 1; i < args.size(); ++i) {
    if (args[i] == "--case" && i + 1 < args.size()) {which = args[i + 1];}
    if (args[i] == "--no-ipc") {ipc = false;}
  }
  if (which == "per-sub-buffer") {per_sub_buffer(ipc);}
  else if (which == "keep-all") {keep_all();}
  else if (which == "transient-local") {transient_local(ipc);}
  else if (which == "reliability") {reliability(ipc);}
  else if (which == "buffer-type") {buffer_type();}
  else if (which == "double-queue") {double_queue();}
  else if (which == "topics") {topics();}
  else if (which == "mixed-transient") {mixed_transient();}
  else if (which == "loan") {loan();}
  else {std::fprintf(stderr, "unknown --case %s\n", which.c_str());}
  rclcpp::shutdown();
  return 0;
}
