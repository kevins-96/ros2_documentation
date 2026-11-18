// 同样 N 个位姿，三种消息结构的序列化 / 反序列化耗时
//   nav_msgs/Path            : N 个 PoseStamped，每个都带 header（时间戳 + frame_id 字符串）
//   geometry_msgs/PoseArray  : N 个 Pose，复合类型数组，但没有字符串
//   sensor_msgs/PointCloud2  : 7 个 float64 字段打包进一个 uint8[]（"结构体数组"变"字节数组"）
// publish() 在同步发布模式下会在调用线程里做序列化，所以这里的耗时会直接加在你的回调上。
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "geometry_msgs/msg/pose_array.hpp"
#include "nav_msgs/msg/path.hpp"
#include "rclcpp/serialization.hpp"
#include "rclcpp/serialized_message.hpp"
#include "sensor_msgs/msg/point_cloud2.hpp"
#include "sensor_msgs/msg/point_field.hpp"

namespace
{
using Clock = std::chrono::steady_clock;

geometry_msgs::msg::Pose make_pose(size_t i)
{
  geometry_msgs::msg::Pose p;
  p.position.x = 0.05 * static_cast<double>(i);
  p.position.y = 0.01 * static_cast<double>(i % 100);
  p.orientation.w = 1.0;
  return p;
}

nav_msgs::msg::Path make_path(size_t n)
{
  nav_msgs::msg::Path msg;
  msg.header.frame_id = "map";
  msg.poses.resize(n);
  for (size_t i = 0; i < n; ++i) {
    msg.poses[i].header.frame_id = "map";
    msg.poses[i].pose = make_pose(i);
  }
  return msg;
}

geometry_msgs::msg::PoseArray make_pose_array(size_t n)
{
  geometry_msgs::msg::PoseArray msg;
  msg.header.frame_id = "map";
  msg.poses.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    msg.poses.push_back(make_pose(i));
  }
  return msg;
}

sensor_msgs::msg::PointCloud2 make_cloud(size_t n)
{
  sensor_msgs::msg::PointCloud2 msg;
  msg.header.frame_id = "map";
  const char * names[] = {"x", "y", "z", "qx", "qy", "qz", "qw"};
  for (uint32_t k = 0; k < 7; ++k) {
    sensor_msgs::msg::PointField f;
    f.name = names[k];
    f.offset = 8 * k;
    f.datatype = sensor_msgs::msg::PointField::FLOAT64;
    f.count = 1;
    msg.fields.push_back(f);
  }
  msg.height = 1;
  msg.width = static_cast<uint32_t>(n);
  msg.point_step = 7 * 8;
  msg.row_step = msg.point_step * msg.width;
  msg.is_dense = true;
  msg.data.resize(msg.row_step);
  for (size_t i = 0; i < n; ++i) {
    const auto p = make_pose(i);
    const double v[7] = {p.position.x, p.position.y, p.position.z,
      p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w};
    std::memcpy(&msg.data[i * msg.point_step], v, sizeof(v));
  }
  return msg;
}

template<typename MsgT>
void bench(const char * label, const MsgT & msg, size_t n, int reps)
{
  rclcpp::Serialization<MsgT> ser;
  rclcpp::SerializedMessage buf;
  MsgT out;

  ser.serialize_message(&msg, &buf);  // 预热：让缓冲区先分配好
  auto t0 = Clock::now();
  for (int r = 0; r < reps; ++r) {
    ser.serialize_message(&msg, &buf);
  }
  auto t1 = Clock::now();
  for (int r = 0; r < reps; ++r) {
    ser.deserialize_message(&buf, &out);
  }
  auto t2 = Clock::now();

  const double s_us = std::chrono::duration<double, std::micro>(t1 - t0).count() / reps;
  const double d_us = std::chrono::duration<double, std::micro>(t2 - t1).count() / reps;
  std::printf("%-12s n=%7zu  bytes=%9zu  serialize=%9.1f us  deserialize=%9.1f us\n",
    label, n, buf.size(), s_us, d_us);
}
}  // namespace

int main(int argc, char ** argv)
{
  std::vector<size_t> sizes = {1000, 10000, 100000};
  if (argc > 1) {
    sizes = {static_cast<size_t>(std::stoul(argv[1]))};
  }
  for (const auto n : sizes) {
    const int reps = n >= 100000 ? 20 : 200;
    bench("Path", make_path(n), n, reps);
    bench("PoseArray", make_pose_array(n), n, reps);
    bench("PointCloud2", make_cloud(n), n, reps);
    std::printf("\n");
  }
  return 0;
}
