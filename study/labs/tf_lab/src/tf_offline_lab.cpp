// 练习「四元数与 tf2 缓冲」：纯计算，不需要任何其他节点。
// 运行：ros2 run tf_lab tf_offline_lab
// 每一节先看注释里的问题，写下预测，再看输出。
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>

#include "geometry_msgs/msg/transform_stamped.hpp"
#include "tf2/LinearMath/Quaternion.hpp"
#include "tf2/LinearMath/Vector3.hpp"
#include "tf2/buffer_core.hpp"
#include "tf2/exceptions.hpp"
#include "tf2/utils.hpp"
#include "tf2_geometry_msgs/tf2_geometry_msgs.hpp"

static double deg(double rad) {return rad * 180.0 / M_PI;}
static void print_v(const char * name, const tf2::Vector3 & v)
{
  std::printf("  %-28s (%6.3f, %6.3f, %6.3f)\n", name, v.x(), v.y(), v.z());
}

int main()
{
  const tf2::Vector3 x_axis(1, 0, 0), y_axis(0, 1, 0);

  std::puts("[1] 乘法顺序：q_yaw = 绕 Z 转 90°，q_roll = 绕 X 转 90°。把 y 轴分别用两种乘积旋转");
  tf2::Quaternion q_yaw, q_roll;
  q_yaw.setRPY(0, 0, M_PI / 2);
  q_roll.setRPY(M_PI / 2, 0, 0);
  print_v("q_yaw * q_roll 作用于 y 轴", tf2::quatRotate(q_yaw * q_roll, y_axis));
  print_v("q_roll * q_yaw 作用于 y 轴", tf2::quatRotate(q_roll * q_yaw, y_axis));

  std::puts("[2] setRPY 是固定轴（fixed axis）RPY：setRPY(r,p,y) 是否等于 Rz(y)*Ry(p)*Rx(r)？");
  tf2::Quaternion q_rpy, qx, qy, qz;
  q_rpy.setRPY(0.3, 0.2, 0.1);
  qx.setRPY(0.3, 0, 0);
  qy.setRPY(0, 0.2, 0);
  qz.setRPY(0, 0, 0.1);
  tf2::Quaternion zyx = qz * qy * qx, xyz = qx * qy * qz;
  std::printf("  setRPY      (%.4f, %.4f, %.4f, %.4f)\n", q_rpy.x(), q_rpy.y(), q_rpy.z(), q_rpy.w());
  std::printf("  qz*qy*qx    (%.4f, %.4f, %.4f, %.4f)\n", zyx.x(), zyx.y(), zyx.z(), zyx.w());
  std::printf("  qx*qy*qz    (%.4f, %.4f, %.4f, %.4f)\n", xyz.x(), xyz.y(), xyz.z(), xyz.w());

  std::puts("[3] q 和 -q：分量不同，旋转是否相同？inverse() 和『四个分量全取负』一样吗？");
  tf2::Quaternion q(0, 0, std::sin(0.4), std::cos(0.4));   // yaw = 0.8 rad
  tf2::Quaternion neg(-q.x(), -q.y(), -q.z(), -q.w());
  print_v("q 作用于 x 轴", tf2::quatRotate(q, x_axis));
  print_v("-q 作用于 x 轴", tf2::quatRotate(neg, x_axis));
  print_v("q.inverse() 作用于 x 轴", tf2::quatRotate(q.inverse(), x_axis));
  std::printf("  getYaw: q=%.1f°  -q=%.1f°  inverse=%.1f°\n",
    deg(tf2::getYaw(q)), deg(tf2::getYaw(neg)), deg(tf2::getYaw(q.inverse())));

  std::puts("[4] 航向差：yaw1 = 170°，yaw2 = -170°，机器人实际要转多少？");
  const double y1 = 170.0 * M_PI / 180, y2 = -170.0 * M_PI / 180;
  tf2::Quaternion q1, q2;
  q1.setRPY(0, 0, y1);
  q2.setRPY(0, 0, y2);
  std::printf("  直接相减 yaw2 - yaw1      = %.1f°\n", deg(y2 - y1));
  std::printf("  四元数 getYaw(q2*q1^-1)   = %.1f°\n", deg(tf2::getYaw(q2 * q1.inverse())));
  std::printf("  atan2(sin d, cos d)        = %.1f°\n", deg(std::atan2(std::sin(y2 - y1), std::cos(y2 - y1))));

  std::puts("[5] 把『坏』四元数放进 tf2::BufferCore，setTransform 返回什么？之后查得到吗？");
  struct Case {const char * name; double x, y, z, w;};
  for (Case c : {Case{"(0,0,0,1) 单位", 0, 0, 0, 1}, Case{"(0,0,0,0) 全零", 0, 0, 0, 0},
      Case{"(0,0,0,2) 未归一化", 0, 0, 0, 2}, Case{"(0,0,0.001,1) 轻微误差", 0, 0, 0.001, 1}})
  {
    tf2::BufferCore core;
    geometry_msgs::msg::TransformStamped t;
    t.header.stamp.sec = 1;
    t.header.frame_id = "odom";
    t.child_frame_id = "base_link";
    t.transform.rotation.x = c.x;
    t.transform.rotation.y = c.y;
    t.transform.rotation.z = c.z;
    t.transform.rotation.w = c.w;
    bool accepted = core.setTransform(t, "lab");
    bool found = core.canTransform("odom", "base_link", tf2::TimePointZero);
    std::printf("  %-22s setTransform=%s  canTransform=%s\n", c.name,
      accepted ? "true" : "false", found ? "true" : "false");
  }

  std::puts("[6] BufferCore 时间语义：t = 0..20 s 每秒一条 odom->base_link，x = t。查不同时刻：");
  tf2::BufferCore core;   // 默认缓存 10 s
  for (int s = 0; s <= 20; ++s) {
    geometry_msgs::msg::TransformStamped t;
    t.header.stamp.sec = 100 + s;
    t.header.frame_id = "odom";
    t.child_frame_id = "base_link";
    t.transform.translation.x = s;
    t.transform.rotation.w = 1.0;
    core.setTransform(t, "lab");
  }
  auto at = [](double sec) {
      return tf2::TimePoint(std::chrono::duration_cast<tf2::Duration>(
               std::chrono::duration<double>(100.0 + sec)));
    };
  for (auto [label, tp] : {std::pair<std::string, tf2::TimePoint>{"TimePointZero", tf2::TimePointZero},
      {"t = 15.5 s", at(15.5)}, {"t = 20.2 s", at(20.2)}, {"t = 5 s", at(5.0)}})
  {
    try {
      auto r = core.lookupTransform("odom", "base_link", tp);
      std::printf("  %-14s x = %.2f\n", label.c_str(), r.transform.translation.x);
    } catch (const tf2::TransformException & ex) {
      std::printf("  %-14s 异常: %s\n", label.c_str(), ex.what());
    }
  }
  return 0;
}
