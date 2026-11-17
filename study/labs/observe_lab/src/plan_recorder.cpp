// 在自己的节点里写 bag：订阅序列化消息，原样写入，不做反序列化
#include <memory>
#include <string>

#include "nav_msgs/msg/path.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rosbag2_cpp/writer.hpp"

class PlanRecorder : public rclcpp::Node
{
public:
  PlanRecorder()
  : Node("plan_recorder")
  {
    const auto uri = declare_parameter("bag", std::string("plan_bag"));
    writer_ = std::make_unique<rosbag2_cpp::Writer>();
    writer_->open(uri);   // 默认存储格式 mcap；目录已存在会抛异常
    // 模板参数写真实消息类型（决定话题类型），回调参数写 SerializedMessage：
    // rclcpp 就把原始字节交给回调，省掉"反序列化再序列化"。
    // 写成 create_subscription<rclcpp::SerializedMessage> 会在运行时抛异常（见练习）
    sub_ = create_subscription<nav_msgs::msg::Path>(
      "plan", 10, [this](std::shared_ptr<const rclcpp::SerializedMessage> msg) {
        // 时间戳用"收到的时间"；也可以用消息 header 里的生成时间
        // 练习：这里故意写相对名 "plan"，bag 里的话题名就是 "plan"（不带 /）
        writer_->write(msg, "plan", "nav_msgs/msg/Path", now());
        if (++count_ % 10 == 0) {
          RCLCPP_INFO(get_logger(), "written %d messages", count_);
        }
      });
    RCLCPP_INFO(get_logger(), "recording /plan into '%s'", uri.c_str());
  }

private:
  std::unique_ptr<rosbag2_cpp::Writer> writer_;   // 析构时关闭 bag、写 metadata.yaml
  rclcpp::Subscription<nav_msgs::msg::Path>::SharedPtr sub_;
  int count_{0};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<PlanRecorder>());
  rclcpp::shutdown();
  return 0;
}
