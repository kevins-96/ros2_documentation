// 练习：设计文档说"rclcpp 要让用户给发布者、订阅者传自定义分配器，实时路径上不再调用系统分配器"。
// Rolling 里做到了多少？
//
// 设计文档：Introduction to Real-time Systems（120）、Proposal for Implementation of
//           Real-time Systems in ROS 2（121，"Memory Allocation"、"Verification"）
//
// 做法：重载全局 operator new，统计"系统分配器"被调用的次数；自定义分配器直接调 malloc，
// 单独计数。每种情况先预热 100 条，再测 1000 条，分别统计 publish() 和 spin_some()（取消息、
// 调回调）两个阶段平均每条消息的次数。
//   --case default      默认分配器，不开进程内通信，按引用发布同一条消息
//   --case custom       发布者、订阅者、消息内存策略都用自定义分配器
//   --case ipc          默认分配器，开进程内通信，每次 make_unique 一条新消息再发布
//   --case custom-ipc   自定义分配器 + 进程内通信，消息也用自定义分配器分配
//   --case atomics      不跑 ROS，只打印几种类型的 std::atomic 是否无锁
//   --trace             额外打印第一条被测消息上每次系统 new 的调用栈（看分配来自哪一层）
//
// 用法：ros2 run design_lab rt_alloc_probe --case custom
// 适用：ROS 2 Rolling（rclcpp 33.x，rmw_fastrtps_cpp）
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <execinfo.h>
#include <memory>
#include <new>
#include <string>

#include "rclcpp/allocator/allocator_common.hpp"
#include "rclcpp/allocator/allocator_deleter.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/int32.hpp"
#include "study_interfaces/msg/pose2_d.hpp"

// ---------- 系统分配器计数：所有 new / make_shared / std::vector 默认都走这里 ----------
static std::atomic<size_t> g_sys_new{0};
static std::atomic<bool> g_trace{false};
static thread_local bool t_in_trace = false;

void * operator new(std::size_t n)
{
  g_sys_new.fetch_add(1, std::memory_order_relaxed);
  if (g_trace.load(std::memory_order_relaxed) && !t_in_trace) {
    t_in_trace = true;  // backtrace() 第一次调用可能自己也分配内存，防止递归
    void * frames[16];
    int k = backtrace(frames, 16);
    std::fprintf(stderr, "---- 系统 new(%zu 字节) 的调用栈 ----\n", n);
    backtrace_symbols_fd(frames + 1, k - 1, 2);
    t_in_trace = false;
  }
  if (void * p = std::malloc(n ? n : 1)) {return p;}
  throw std::bad_alloc();
}
void operator delete(void * p) noexcept {std::free(p);}
void operator delete(void * p, std::size_t) noexcept {std::free(p);}

// ---------- 自定义分配器：满足 C++ Allocator 要求的最小实现，直接调 malloc 并计数 ----------
// 真实项目里这里换成内存池或 TLSF（realtime_support/tlsf_cpp），做法完全一样。
static std::atomic<size_t> g_custom{0};

template<typename T>
struct CountingAllocator
{
  using value_type = T;
  CountingAllocator() noexcept = default;
  template<typename U>
  CountingAllocator(const CountingAllocator<U> &) noexcept {}
  T * allocate(std::size_t n)
  {
    g_custom.fetch_add(1, std::memory_order_relaxed);
    return static_cast<T *>(std::malloc(n * sizeof(T)));
  }
  void deallocate(T * p, std::size_t) noexcept {std::free(p);}
  template<typename U>
  bool operator==(const CountingAllocator<U> &) const noexcept {return true;}
  template<typename U>
  bool operator!=(const CountingAllocator<U> &) const noexcept {return false;}
};

using Msg = std_msgs::msg::Int32;
constexpr int kWarmup = 100;
constexpr int kMeasure = 1000;
static bool g_want_trace = false;

struct Counts
{
  size_t sys_pub = 0, custom_pub = 0, sys_spin = 0, custom_spin = 0;
  int received = 0;
};

static void report(const char * name, const Counts & c)
{
  std::printf(
    "%-11s publish(): 系统 new %.2f 次/条, 自定义 %.2f 次/条 | spin_some(): 系统 new %.2f 次/条, "
    "自定义 %.2f 次/条 | 收到 %d/%d\n",
    name, double(c.sys_pub) / kMeasure, double(c.custom_pub) / kMeasure,
    double(c.sys_spin) / kMeasure, double(c.custom_spin) / kMeasure, c.received, kMeasure);
}

// 发布一条、spin_some 一次，分别统计两个阶段
template<typename PublishFn>
static Counts run_loop(rclcpp::Executor & exec, PublishFn publish_one, int & received)
{
  Counts c;
  for (int i = 0; i < kWarmup + kMeasure; ++i) {
    if (i == kWarmup) {received = 0;}
    const bool trace = g_want_trace && i == kWarmup;
    if (trace) {std::fprintf(stderr, "======== publish() ========\n"); g_trace = true;}
    const size_t s0 = g_sys_new.load(), c0 = g_custom.load();
    publish_one(i);
    const size_t s1 = g_sys_new.load(), c1 = g_custom.load();
    if (trace) {std::fprintf(stderr, "======== spin_some() ========\n");}
    exec.spin_some();
    const size_t s2 = g_sys_new.load(), c2 = g_custom.load();
    g_trace = false;
    if (i >= kWarmup) {
      c.sys_pub += s1 - s0; c.custom_pub += c1 - c0;
      c.sys_spin += s2 - s1; c.custom_spin += c2 - c1;
    }
  }
  c.received = received;
  return c;
}

static void run_default(bool ipc)
{
  auto node = std::make_shared<rclcpp::Node>(
    "rt_alloc_probe", rclcpp::NodeOptions().use_intra_process_comms(ipc));
  int received = 0;
  auto pub = node->create_publisher<Msg>("rt_alloc", 10);
  auto sub = node->create_subscription<Msg>(
    "rt_alloc", 10, [&received](Msg::ConstSharedPtr) {++received;});
  rclcpp::executors::SingleThreadedExecutor exec;
  exec.add_node(node);
  rclcpp::sleep_for(std::chrono::milliseconds(300));  // 等发现完成
  Msg reused;
  auto c = run_loop(exec, [&](int i) {
        if (ipc) {
          auto m = std::make_unique<Msg>();
          m->data = i;
          pub->publish(std::move(m));
        } else {
          reused.data = i;
          pub->publish(reused);
        }
      }, received);
  report(ipc ? "ipc" : "default", c);
}

static void run_custom(bool ipc)
{
  using Alloc = CountingAllocator<void>;
  auto alloc = std::make_shared<Alloc>();
  auto node = std::make_shared<rclcpp::Node>(
    "rt_alloc_probe", rclcpp::NodeOptions().use_intra_process_comms(ipc));
  int received = 0;

  rclcpp::PublisherOptionsWithAllocator<Alloc> pub_opts;
  pub_opts.allocator = alloc;
  auto pub = node->create_publisher<Msg>("rt_alloc", 10, pub_opts);

  rclcpp::SubscriptionOptionsWithAllocator<Alloc> sub_opts;
  sub_opts.allocator = alloc;
  auto mem_strategy = std::make_shared<
    rclcpp::message_memory_strategy::MessageMemoryStrategy<Msg, Alloc>>(alloc);
  auto sub = node->create_subscription<Msg>(
    "rt_alloc", 10, [&received](Msg::ConstSharedPtr) {++received;}, sub_opts, mem_strategy);

  rclcpp::executors::SingleThreadedExecutor exec;
  exec.add_node(node);
  rclcpp::sleep_for(std::chrono::milliseconds(300));

  // 用同一个分配器分配要交出所有权的消息（与 tlsf_cpp 的 allocator_example 相同写法）
  using MsgAllocTraits = rclcpp::allocator::AllocRebind<Msg, Alloc>;
  using MsgAlloc = MsgAllocTraits::allocator_type;
  using MsgDeleter = rclcpp::allocator::Deleter<MsgAlloc, Msg>;
  MsgAlloc msg_alloc = *alloc;
  MsgDeleter deleter;
  rclcpp::allocator::set_allocator_for_deleter(&deleter, &msg_alloc);

  Msg reused;
  auto c = run_loop(exec, [&](int i) {
        if (ipc) {
          Msg * p = MsgAllocTraits::allocate(msg_alloc, 1);
          MsgAllocTraits::construct(msg_alloc, p);
          std::unique_ptr<Msg, MsgDeleter> m(p, deleter);
          m->data = i;
          pub->publish(std::move(m));
        } else {
          reused.data = i;
          pub->publish(reused);
        }
      }, received);
  report(ipc ? "custom-ipc" : "custom", c);
}

static void print_atomics()
{
  // 设计文档 121："优先用原子量代替互斥锁；但很多原子类型因为体系结构限制不是无锁的"
  struct Pose3d {double x, y, yaw;};
  std::printf("std::atomic<bool>::is_always_lock_free      = %d\n",
    int(std::atomic<bool>::is_always_lock_free));
  std::printf("std::atomic<double>::is_always_lock_free    = %d\n",
    int(std::atomic<double>::is_always_lock_free));
  std::printf("std::atomic<int64_t>::is_always_lock_free   = %d\n",
    int(std::atomic<int64_t>::is_always_lock_free));
  std::atomic<Pose3d> pose{};
  std::printf("std::atomic<Pose3d{3 x double}>.is_lock_free() = %d（%zu 字节）\n",
    int(pose.is_lock_free()), sizeof(Pose3d));
  std::atomic<study_interfaces::msg::Pose2D *> ptr{nullptr};
  std::printf("std::atomic<Pose2D *>.is_lock_free()       = %d\n", int(ptr.is_lock_free()));
}

int main(int argc, char ** argv)
{
  auto args = rclcpp::init_and_remove_ros_arguments(argc, argv);
  std::string which = "default";
  for (size_t i = 1; i < args.size(); ++i) {
    if (args[i] == "--case" && i + 1 < args.size()) {which = args[i + 1];}
    if (args[i] == "--trace") {g_want_trace = true;}
  }
  if (which == "default") {run_default(false);}
  else if (which == "ipc") {run_default(true);}
  else if (which == "custom") {run_custom(false);}
  else if (which == "custom-ipc") {run_custom(true);}
  else if (which == "atomics") {print_atomics();}
  else {std::fprintf(stderr, "unknown --case %s\n", which.c_str());}
  rclcpp::shutdown();
  return 0;
}
