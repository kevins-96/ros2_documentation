// 卡片「tf2 坐标变换」最小代码：一个节点同时发布
//   静态变换 base_link -> laser（安装位置，不随时间变化）
//   动态变换 odom -> base_link（机器人绕圆走，20 Hz）
// 运行：ros2 run tf_lab card_tf2_min
#include <chrono>
#include <cmath>
#include <memory>

#include "geometry_msgs/msg/transform_stamped.hpp"
#include "rclcpp/rclcpp.hpp"
#include "tf2/LinearMath/Quaternion.hpp"
#include "tf2_geometry_msgs/tf2_geometry_msgs.hpp"
#include "tf2_ros/static_transform_broadcaster.hpp"
#include "tf2_ros/transform_broadcaster.hpp"

using namespace std::chrono_literals;

class FramePublisher : public rclcpp::Node
{
public:
  FramePublisher()
  : Node("frame_publisher")
  {
    // 静态变换：发一次即可，/tf_static 用 TransientLocal，后加入的监听者也能收到
    static_broadcaster_ = std::make_shared<tf2_ros::StaticTransformBroadcaster>(*this);
    geometry_msgs::msg::TransformStamped laser;
    laser.header.stamp = now();
    laser.header.frame_id = "base_link";   // 父
    laser.child_frame_id = "laser";        // 子
    laser.transform.translation.x = 0.2;
    laser.transform.translation.z = 0.15;
    laser.transform.rotation.w = 1.0;      // 单位四元数，别全写 0
    static_broadcaster_->sendTransform(laser);

    // 动态变换：每次都要带当前时间戳
    broadcaster_ = std::make_unique<tf2_ros::TransformBroadcaster>(*this);
    timer_ = create_wall_timer(50ms, [this]() {publish_odom();});
  }

private:
  void publish_odom()
  {
    const double t = now().seconds();
    geometry_msgs::msg::TransformStamped tf;
    tf.header.stamp = now();
    tf.header.frame_id = "odom";
    tf.child_frame_id = "base_link";
    tf.transform.translation.x = std::cos(0.5 * t);
    tf.transform.translation.y = std::sin(0.5 * t);
    tf2::Quaternion q;
    q.setRPY(0.0, 0.0, 0.5 * t + M_PI / 2);  // 朝向切线方向
    tf.transform.rotation = tf2::toMsg(q);
    broadcaster_->sendTransform(tf);
  }

  std::shared_ptr<tf2_ros::StaticTransformBroadcaster> static_broadcaster_;
  std::unique_ptr<tf2_ros::TransformBroadcaster> broadcaster_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<FramePublisher>());
  rclcpp::shutdown();
  return 0;
}
