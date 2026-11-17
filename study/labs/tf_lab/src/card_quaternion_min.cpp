// 卡片「四元数」最小代码：RPY <-> 四元数、消息互转、取 yaw、相对旋转
// 运行：ros2 run tf_lab card_quaternion_min
#include <cmath>
#include <cstdio>

#include "geometry_msgs/msg/quaternion.hpp"
#include "tf2/LinearMath/Matrix3x3.hpp"
#include "tf2/LinearMath/Quaternion.hpp"
#include "tf2/utils.hpp"                                // tf2::getYaw
#include "tf2_geometry_msgs/tf2_geometry_msgs.hpp"      // tf2::toMsg / fromMsg

int main()
{
  // 1) 用 RPY 思考，再转成四元数（ROS 顺序 x, y, z, w；w 在最后）
  tf2::Quaternion q;
  q.setRPY(0.0, 0.0, M_PI / 2);                 // 原地左转 90°
  std::printf("q = (%.3f, %.3f, %.3f, %.3f)\n", q.x(), q.y(), q.z(), q.w());

  // 2) tf2 类型 <-> 消息类型
  geometry_msgs::msg::Quaternion msg = tf2::toMsg(q);
  tf2::Quaternion back;
  tf2::fromMsg(msg, back);

  // 3) 平面导航最常用：从四元数取 yaw
  double yaw = tf2::getYaw(msg);
  double r, p, y;
  tf2::Matrix3x3(back).getRPY(r, p, y);         // 完整 RPY
  std::printf("yaw = %.3f rad (getRPY: %.3f)\n", yaw, y);

  // 4) 相对旋转：q2 = q_r * q1  =>  q_r = q2 * q1^-1
  tf2::Quaternion q1, q2;
  q1.setRPY(0, 0, 0.3);
  q2.setRPY(0, 0, 1.0);
  tf2::Quaternion q_r = q2 * q1.inverse();
  q_r.normalize();                              // 连乘后归一化，防止误差累积
  std::printf("relative yaw = %.3f rad\n", tf2::getYaw(q_r));
  return 0;
}
