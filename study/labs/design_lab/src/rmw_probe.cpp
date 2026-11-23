// 练习：rmw 中间件接口、序列化、唯一网络流（unique network flows）的设计说法在 Rolling 上的样子
//
// 设计文档：ROS 2 middleware interface（060）、ROS 2 Message Research（serialization）、
//           Unique Network Flows（2021-05）
//
//   --case identity   rmw 实现名、序列化格式、每个句柄里带的实现标识
//   --case native     通过 rmw_fastrtps_cpp 的扩展 API 拿到 Fast DDS 原生 DataWriter，读 ROS API 没暴露的 QoS
//   --case serialize  CDR 序列化后的字节数；拷贝 vs 序列化 vs 反序列化的耗时
//   --case generic    运行时才知道类型：用类型名字符串创建发布者 / 订阅者
//   --case flows      require_unique_network_flow_endpoints 的四个取值，和各自拿到的网络流端点
//
// 用法：ros2 run design_lab rmw_probe --case serialize
// 适用：ROS 2 Rolling（rclcpp 33.x，rmw_fastrtps_cpp 9.x，Fast DDS 3.x）
#include <chrono>
#include <cstdio>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "fastdds/dds/publisher/DataWriter.hpp"
#include "fastdds/dds/publisher/qos/DataWriterQos.hpp"
#include "geometry_msgs/msg/pose_array.hpp"
#include "rcl/publisher.h"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp/serialization.hpp"
#include "rmw/rmw.h"
#include "rmw_fastrtps_cpp/get_publisher.hpp"
#include "std_msgs/msg/int32.hpp"
#include "std_msgs/msg/string.hpp"
#include "study_interfaces/msg/fixed_block.hpp"

using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;

static void identity(rclcpp::Node::SharedPtr node)
{
  auto pub = node->create_publisher<std_msgs::msg::Int32>("rmw_probe", 10);
  rmw_publisher_t * rmw_pub = rcl_publisher_get_rmw_handle(pub->get_publisher_handle().get());
  std::printf("rmw_get_implementation_identifier() = %s\n", rmw_get_implementation_identifier());
  std::printf("rmw_get_serialization_format()      = %s\n", rmw_get_serialization_format());
  std::printf("rmw_publisher_t::implementation_identifier = %s\n",
    rmw_pub->implementation_identifier);
  std::printf("rmw_publisher_t::topic_name = %s，data 指针（不透明）= %p\n",
    rmw_pub->topic_name, rmw_pub->data);
}

template<typename MsgT>
static void print_native(rclcpp::Node::SharedPtr node, const std::string & topic)
{
  auto pub = node->create_publisher<MsgT>(topic, rclcpp::QoS(7));
  rmw_publisher_t * rmw_pub = rcl_publisher_get_rmw_handle(pub->get_publisher_handle().get());
  eprosima::fastdds::dds::DataWriter * dw = rmw_fastrtps_cpp::get_datawriter(rmw_pub);
  if (!dw) {
    std::printf("get_datawriter 返回 NULL（当前 RMW 不是 rmw_fastrtps_cpp？）\n");
    return;
  }
  const auto qos = dw->get_qos();
  std::printf("原生 DataWriter：类型名 %s\n", dw->get_type().get_type_name().c_str());
  std::printf("  history.depth = %d（ROS 里给的 7）\n", qos.history().depth);
  std::printf("  publish_mode  = %s（ROS QoS 里没有这一项）\n",
    qos.publish_mode().kind == eprosima::fastdds::dds::SYNCHRONOUS_PUBLISH_MODE ?
    "SYNCHRONOUS" : "ASYNCHRONOUS");
  std::printf("  data_sharing  = %s（ROS QoS 里没有这一项）\n",
    qos.data_sharing().kind() == eprosima::fastdds::dds::OFF ? "OFF" :
    qos.data_sharing().kind() == eprosima::fastdds::dds::ON ? "ON" : "AUTO");
  std::printf("  resource_limits.max_samples = %d\n", qos.resource_limits().max_samples);
}

static void native(rclcpp::Node::SharedPtr node)
{
  print_native<std_msgs::msg::Int32>(node, "rmw_probe");
  print_native<std_msgs::msg::String>(node, "rmw_probe_str");
  print_native<study_interfaces::msg::FixedBlock>(node, "rmw_probe_block");
}

template<typename MsgT>
static size_t serialized_size(const MsgT & msg)
{
  rclcpp::Serialization<MsgT> ser;
  rclcpp::SerializedMessage out;
  ser.serialize_message(&msg, &out);
  return out.size();
}

// 平均耗时（μs）：拷贝一份 / 序列化 / 反序列化
template<typename MsgT>
static void time_it(const char * name, const MsgT & msg, int n)
{
  rclcpp::Serialization<MsgT> ser;
  rclcpp::SerializedMessage buf;
  ser.serialize_message(&msg, &buf);  // 预热，也让缓冲区先分配好
  auto copy = std::make_unique<MsgT>();
  auto back = std::make_unique<MsgT>();

  auto t0 = Clock::now();
  for (int i = 0; i < n; ++i) {*copy = msg;}
  auto t1 = Clock::now();
  for (int i = 0; i < n; ++i) {ser.serialize_message(&msg, &buf);}
  auto t2 = Clock::now();
  for (int i = 0; i < n; ++i) {ser.deserialize_message(&buf, back.get());}
  auto t3 = Clock::now();
  auto us = [n](auto d) {
      return std::chrono::duration<double, std::micro>(d).count() / n;
    };
  std::printf("%-26s %9zu 字节 | 拷贝 %8.1f μs | 序列化 %8.1f μs | 反序列化 %8.1f μs | "
    "序列化/拷贝 = %.1f 倍\n", name, buf.size(), us(t1 - t0), us(t2 - t1), us(t3 - t2),
    us(t2 - t1) / us(t1 - t0));
}

static void serialize()
{
  std_msgs::msg::Int32 i32;
  i32.data = 7;
  std_msgs::msg::String s;
  s.data = "hello";
  std::printf("CDR 字节数：Int32 = %zu，String(\"hello\") = %zu，空 String = %zu\n",
    serialized_size(i32), serialized_size(s), serialized_size(std_msgs::msg::String()));

  auto block = std::make_unique<study_interfaces::msg::FixedBlock>();
  for (size_t i = 0; i < block->data.size(); ++i) {block->data[i] = float(i);}
  time_it("FixedBlock（1 MB 定长）", *block, 200);

  geometry_msgs::msg::PoseArray poses;   // 每个位姿 7 个 double，字段很多、单个很小
  poses.poses.resize(18724);             // 约 1 MB
  for (size_t i = 0; i < poses.poses.size(); ++i) {poses.poses[i].position.x = double(i);}
  time_it("PoseArray（18724 个位姿）", poses, 200);
}

static void generic(rclcpp::Node::SharedPtr node)
{
  // 发布端只给类型名字符串，不 include 任何消息头文件也能发
  auto gpub = node->create_generic_publisher("generic_chatter", "std_msgs/msg/String", 10);
  std::string typed_got;
  auto typed_sub = node->create_subscription<std_msgs::msg::String>("generic_chatter", 10,
      [&typed_got](std_msgs::msg::String::ConstSharedPtr m) {typed_got = m->data;});
  size_t generic_bytes = 0;
  auto gsub = node->create_generic_subscription("generic_chatter", "std_msgs/msg/String", 10,
      [&generic_bytes](std::shared_ptr<const rclcpp::SerializedMessage> m) {
        generic_bytes = m->size();
      });
  rclcpp::executors::SingleThreadedExecutor exec;
  exec.add_node(node);
  rclcpp::sleep_for(500ms);

  std_msgs::msg::String s;
  s.data = "from generic publisher";
  rclcpp::SerializedMessage bytes;
  rclcpp::Serialization<std_msgs::msg::String>().serialize_message(&s, &bytes);
  gpub->publish(bytes);
  for (int i = 0; i < 20; ++i) {exec.spin_some(); rclcpp::sleep_for(10ms);}
  std::printf("通用发布者发出 %zu 字节；类型化订阅者收到 \"%s\"；通用订阅者收到 %zu 字节\n",
    bytes.size(), typed_got.c_str(), generic_bytes);
  try {
    auto bad = node->create_generic_publisher("generic_bad", "std_msgs/msg/NoSuchType", 10);
    std::printf("不存在的类型名：创建成功\n");
  } catch (const std::exception & e) {
    std::printf("不存在的类型名：抛异常 %s\n", e.what());
  }
}

static std::string nfe_str(const std::vector<rclcpp::NetworkFlowEndpoint> & v)
{
  if (v.empty()) {return "（空）";}
  std::ostringstream os;
  for (const auto & e : v) {
    os << "\n      " << e.transport_protocol() << "/" << e.internet_protocol() << " " <<
      e.internet_address() << ":" << e.transport_port() << " flow_label=" << e.flow_label() <<
      " dscp=" << int(e.dscp());
  }
  return os.str();
}

static void flows(rclcpp::Node::SharedPtr node)
{
  const std::pair<rmw_unique_network_flow_endpoints_requirement_t, const char *> reqs[] = {
    {RMW_UNIQUE_NETWORK_FLOW_ENDPOINTS_NOT_REQUIRED, "NOT_REQUIRED"},
    {RMW_UNIQUE_NETWORK_FLOW_ENDPOINTS_STRICTLY_REQUIRED, "STRICTLY_REQUIRED"},
    {RMW_UNIQUE_NETWORK_FLOW_ENDPOINTS_OPTIONALLY_REQUIRED, "OPTIONALLY_REQUIRED"},
    {RMW_UNIQUE_NETWORK_FLOW_ENDPOINTS_SYSTEM_DEFAULT, "SYSTEM_DEFAULT"},
  };
  int k = 0;
  for (const auto & [req, name] : reqs) {
    const std::string topic = "flow_" + std::to_string(k++);
    try {
      rclcpp::PublisherOptions po;
      po.require_unique_network_flow_endpoints = req;
      auto pub = node->create_publisher<std_msgs::msg::Int32>(topic, 10, po);
      std::printf("发布者 %-20s 端点：%s\n", name, nfe_str(pub->get_network_flow_endpoints()).c_str());
    } catch (const std::exception & e) {
      std::printf("发布者 %-20s 创建失败：%s\n", name, e.what());
    }
    try {
      rclcpp::SubscriptionOptions so;
      so.require_unique_network_flow_endpoints = req;
      auto sub = node->create_subscription<std_msgs::msg::Int32>(topic, 10,
          [](std_msgs::msg::Int32::ConstSharedPtr) {}, so);
      std::printf("订阅者 %-20s 端点：%s\n", name, nfe_str(sub->get_network_flow_endpoints()).c_str());
    } catch (const std::exception & e) {
      std::printf("订阅者 %-20s 创建失败：%s\n", name, e.what());
    }
  }
}

int main(int argc, char ** argv)
{
  auto args = rclcpp::init_and_remove_ros_arguments(argc, argv);
  std::string which = "identity";
  for (size_t i = 1; i + 1 < args.size(); ++i) {
    if (args[i] == "--case") {which = args[i + 1];}
  }
  auto node = std::make_shared<rclcpp::Node>("rmw_probe");
  if (which == "identity") {identity(node);}
  else if (which == "native") {native(node);}
  else if (which == "serialize") {serialize();}
  else if (which == "generic") {generic(node);}
  else if (which == "flows") {flows(node);}
  else {std::fprintf(stderr, "unknown --case %s\n", which.c_str());}
  node.reset();
  rclcpp::shutdown();
  return 0;
}
