// 练习：生成的 C++ 消息结构体，构造时到底做了什么？
//
// 设计文档：Generated C++ interfaces（Constructors / MessageInitialization）、
//           IDL - Interface Definition and Language Mapping（Constraint Checking）
// FixedGrid.msg 里有一个 float32[262144] 的定长数组（约 1 MB）和一个带默认值的字段：
//     float64 resolution 0.05
//   1-2. 对比四种初始化方式的字段值和耗时
//   3.   生成代码只有 setter 链，没有按位置传字段的构造函数
//   4.   有界数组、有界字符串的长度约束分别在什么时候检查
//
// 用法：ros2 run design_lab msg_init_bench
// 适用：ROS 2 Rolling（rosidl_generator_cpp 5.x，rmw_fastrtps_cpp）
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <string>

#include "rclcpp/serialization.hpp"
#include "rclcpp/serialized_message.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"
#include "study_interfaces/msg/path_status.hpp"
#include "study_interfaces/msg/fixed_grid.hpp"

using FixedGrid = study_interfaces::msg::FixedGrid;
using Init = rosidl_runtime_cpp::MessageInitialization;

namespace
{
// 预先分配并"弄脏"的一块内存，反复在上面 placement new。
// 这模拟消息池 / 借用消息（loaned message）的场景，也避免了
// "new + 清零 被编译器换成 calloc、拿到的是还没真正写过的零页" 让测量失真。
unsigned char * g_buf = nullptr;
volatile float g_sink = 0.0f;

double bench(Init how, int n)
{
  auto t0 = std::chrono::steady_clock::now();
  for (int i = 0; i < n; ++i) {
    auto * msg = new (g_buf) FixedGrid(how);
    // 编译器屏障（GCC/Clang）：告诉编译器"这块内存可能被读"，
    // 否则它会发现 300 次清零里前 299 次都被下一次覆盖，直接删掉
    asm volatile("" : : "r"(msg) : "memory");
    msg->cells[i % 16] = 1.0f;  // 写一个元素再读回（不读未初始化的数据）
    g_sink = g_sink + msg->cells[i % 16];
    msg->~FixedGrid();
  }
  auto dt = std::chrono::steady_clock::now() - t0;
  return std::chrono::duration<double, std::micro>(dt).count() / n;
}
}  // namespace

int main()
{
  constexpr int kN = 300;
  std::printf("FixedGrid: cells 有 %zu 个 float（%.1f MB），resolution 默认值 0.05\n",
    std::tuple_size<FixedGrid::_cells_type>::value, sizeof(FixedGrid::_cells_type) / 1e6);

  // 1. 各种初始化方式下字段的值（SKIP / DEFAULTS_ONLY 时 cells 未初始化，不能读）
  // 1 MB 的对象放在堆上，别压栈
  auto all_p = std::make_unique<FixedGrid>();  // 默认就是 ALL
  auto zero_p = std::make_unique<FixedGrid>(Init::ZERO);
  auto def_p = std::make_unique<FixedGrid>(Init::DEFAULTS_ONLY);
  const auto & all = *all_p;
  const auto & zero = *zero_p;
  const auto & defaults = *def_p;
  std::printf("ALL:           resolution=%.2f cells[0]=%.1f\n", all.resolution, all.cells[0]);
  std::printf("ZERO:          resolution=%.2f cells[0]=%.1f\n", zero.resolution, zero.cells[0]);
  std::printf("DEFAULTS_ONLY: resolution=%.2f （cells 未初始化）\n", defaults.resolution);

  // 2. 耗时
  g_buf = static_cast<unsigned char *>(::operator new(sizeof(FixedGrid)));
  std::memset(g_buf, 0xAB, sizeof(FixedGrid));
  std::printf("每次构造平均耗时（%d 次）：\n", kN);
  std::printf("  ALL           %8.1f us\n", bench(Init::ALL, kN));
  std::printf("  ZERO          %8.1f us\n", bench(Init::ZERO, kN));
  std::printf("  DEFAULTS_ONLY %8.1f us\n", bench(Init::DEFAULTS_ONLY, kN));
  std::printf("  SKIP          %8.1f us\n", bench(Init::SKIP, kN));
  ::operator delete(g_buf);

  // 3. setter 链：生成代码没有"按位置传字段"的构造函数，只有 set__xxx
  auto g = std::make_unique<FixedGrid>(Init::ZERO);
  g->set__width(512).set__height(512).set__resolution(0.1);
  std::printf("setter 链：width=%u height=%u resolution=%.2f\n", g->width, g->height, g->resolution);

  // 3b. builder：按字段顺序逐个给值，少一个就拿不到消息（编译不过）
  auto ps_built = study_interfaces::build<study_interfaces::msg::PathStatus>()
    .header(std_msgs::msg::Header())
    .status(study_interfaces::msg::PathStatus::STATUS_OK)
    .length_m(3.5f)
    .waypoints({});
  std::printf("builder：status=%u length_m=%.1f\n", ps_built.status, ps_built.length_m);

  // 4. 约束什么时候检查：有界数组 vs 有界字符串
  study_interfaces::msg::PathStatus ps;  // waypoints 是 Pose2D[<=1000]
  try {
    ps.waypoints.resize(1001);
    std::printf("有界数组 [<=1000] resize(1001)：没有报错\n");
  } catch (const std::exception & e) {
    std::printf("有界数组 [<=1000] resize(1001)：立刻抛出 %s\n", e.what());
  }
  g->tag = "this_tag_is_too_long";  // string<=8，赋值时没有任何检查
  std::printf("有界字符串 string<=8 赋值 %zu 个字符：没有报错\n", g->tag.size());
  rclcpp::Serialization<FixedGrid> ser;
  rclcpp::SerializedMessage out;
  try {
    ser.serialize_message(g.get(), &out);
    std::printf("  序列化（发布时也走这一步）：没有报错，%zu 字节\n", out.size());
    auto back = std::make_unique<FixedGrid>(Init::SKIP);
    ser.deserialize_message(&out, back.get());
    std::printf("  反序列化（订阅端）：没有报错，tag=\"%s\"（%zu 个字符）\n",
      back->tag.c_str(), back->tag.size());
  } catch (const std::exception & e) {
    std::string what = e.what();
    std::printf("  序列化（发布时也走这一步）：抛出 %s\n", what.substr(0, what.find('\n')).c_str());
  }
  return 0;
}
