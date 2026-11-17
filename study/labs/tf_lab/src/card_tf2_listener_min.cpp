// 卡片「tf2 坐标变换」最小代码：监听 + 把激光坐标系里的点变换到 odom
// 先运行 card_tf2_min，再运行：ros2 run tf_lab card_tf2_listener_min
#include <chrono>
#include <memory>

#include "geometry_msgs/msg/point_stamped.hpp"
#include "rclcpp/rclcpp.hpp"
#include "tf2/exceptions.hpp"
#include "tf2_geometry_msgs/tf2_geometry_msgs.hpp"   // 提供 PointStamped 的 doTransform
#include "tf2_ros/buffer.hpp"
#include "tf2_ros/transform_listener.hpp"

using namespace std::chrono_literals;

class ObstacleLocator : public rclcpp::Node
{
public:
  ObstacleLocator()
  : Node("obstacle_locator")
  {
    tf_buffer_ = std::make_unique<tf2_ros::Buffer>(get_clock());
    // 传 *this（节点接口）：订阅建在本节点上，遵守本节点的命名空间和重映射
    tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_, *this);
    timer_ = create_wall_timer(500ms, [this]() {on_timer();});
  }

private:
  void on_timer()
  {
    // 激光坐标系前方 1 m 处有一个障碍物
    geometry_msgs::msg::PointStamped in;
    in.header.frame_id = "laser";
    in.header.stamp = rclcpp::Time(0);   // 时间 0 = 用 buffer 里最新的变换
    in.point.x = 1.0;

    try {
      // 1) 直接查变换：返回「source 坐标系在 target 坐标系里的位姿」
      auto t = tf_buffer_->lookupTransform("odom", "laser", tf2::TimePointZero);
      // 2) 更推荐：transform() 从消息里读 source frame，并把结果 frame_id 写成 target
      auto out = tf_buffer_->transform(in, "odom");
      RCLCPP_INFO(
        get_logger(), "laser in odom: (%.2f, %.2f) | obstacle in %s: (%.2f, %.2f)",
        t.transform.translation.x, t.transform.translation.y,
        out.header.frame_id.c_str(), out.point.x, out.point.y);
    } catch (const tf2::TransformException & ex) {
      RCLCPP_WARN(get_logger(), "%s", ex.what());
    }
  }

  std::unique_ptr<tf2_ros::Buffer> tf_buffer_;
  std::shared_ptr<tf2_ros::TransformListener> tf_listener_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<ObstacleLocator>());
  rclcpp::shutdown();
  return 0;
}
