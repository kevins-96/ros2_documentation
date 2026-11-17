// 卡片「tf2 与时间」最小代码：tf2_ros::MessageFilter
// 收到带时间戳的传感器消息后先缓存，等到「该时刻的变换」可用再回调。
// 运行：card_tf2_min 发布 TF；然后
//   ros2 run tf_lab card_tf2_filter_min
//   ros2 topic pub -r 5 /obstacle geometry_msgs/msg/PointStamped
//     "{header: {stamp: now, frame_id: laser}, point: {x: 1.0}}"
//   （两行连起来写成一条命令）
#include <chrono>
#include <memory>
#include <string>

#include "geometry_msgs/msg/point_stamped.hpp"
#include "message_filters/subscriber.hpp"
#include "rclcpp/rclcpp.hpp"
#include "tf2_geometry_msgs/tf2_geometry_msgs.hpp"
#include "tf2_ros/buffer.hpp"
#include "tf2_ros/create_timer_ros.hpp"
#include "tf2_ros/message_filter.hpp"
#include "tf2_ros/transform_listener.hpp"

using namespace std::chrono_literals;
using PointStamped = geometry_msgs::msg::PointStamped;

class ObstacleFilter : public rclcpp::Node
{
public:
  ObstacleFilter()
  : Node("obstacle_filter")
  {
    buffer_ = std::make_shared<tf2_ros::Buffer>(get_clock());
    // MessageFilter 内部要用定时器等变换，必须先给 Buffer 设置定时器接口
    buffer_->setCreateTimerInterface(
      std::make_shared<tf2_ros::CreateTimerROS>(*this));   // Rolling：传节点
    listener_ = std::make_shared<tf2_ros::TransformListener>(*buffer_, *this);

    sub_.subscribe(this, "obstacle", rclcpp::SensorDataQoS());   // Rolling：必须给 QoS
    filter_ = std::make_shared<tf2_ros::MessageFilter<PointStamped>>(
      sub_, *buffer_, target_frame_, 10,            // 队列 10 条
      *this,                                        // Rolling：传节点（NodeInterfaces）
      std::chrono::milliseconds(500));              // 最多等 0.5 s，超时丢弃
    filter_->registerCallback(&ObstacleFilter::on_point, this);
  }

private:
  void on_point(const PointStamped::ConstSharedPtr & msg)
  {
    // 到这里时，msg->header.stamp 时刻的变换已经可用
    PointStamped out = buffer_->transform(*msg, target_frame_);
    RCLCPP_INFO(get_logger(), "obstacle in %s: (%.2f, %.2f)",
      target_frame_.c_str(), out.point.x, out.point.y);
  }

  std::string target_frame_ = "odom";
  std::shared_ptr<tf2_ros::Buffer> buffer_;
  std::shared_ptr<tf2_ros::TransformListener> listener_;
  message_filters::Subscriber<PointStamped> sub_;
  std::shared_ptr<tf2_ros::MessageFilter<PointStamped>> filter_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<ObstacleFilter>());
  rclcpp::shutdown();
  return 0;
}
