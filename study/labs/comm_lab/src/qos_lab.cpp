// 练习：QoS 不匹配时到底发生了什么？
//
//   --case be-pub-rel-sub   发布端 BestEffort，订阅端 Reliable          （预测：能通信吗？）
//   --case rel-pub-be-sub   发布端 Reliable，订阅端 BestEffort          （预测：能通信吗？）
//   --case late-volatile    TransientLocal 发布端只发 1 次"地图"，1 s 后才创建 Volatile 订阅
//   --case late-transient   同上，但订阅端也是 TransientLocal
//   --case deadline         双方约定 Deadline 100 ms，发布端实际只有 5 Hz
//
// 程序运行约 3 s 后打印：收到的消息数、各类 QoS 事件次数。
// 适用：ROS 2 Rolling（rclcpp）
#include <chrono>
#include <cstring>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

using namespace std::chrono_literals;
using Msg = std_msgs::msg::String;

class QosLab : public rclcpp::Node
{
public:
  explicit QosLab(const std::string & c)
  : Node("qos_lab"), case_(c)
  {
    rclcpp::QoS pub_qos(10);
    rclcpp::QoS sub_qos(10);
    bool late_sub = false;
    auto period = 50ms;

    if (c == "be-pub-rel-sub") {
      pub_qos.best_effort();
      sub_qos.reliable();
    } else if (c == "rel-pub-be-sub") {
      pub_qos.reliable();
      sub_qos.best_effort();
    } else if (c == "late-volatile" || c == "late-transient") {
      pub_qos = rclcpp::QoS(1).reliable().transient_local();
      sub_qos = rclcpp::QoS(1).reliable();
      if (c == "late-transient") {sub_qos.transient_local();}
      late_sub = true;
    } else if (c == "deadline") {
      pub_qos.deadline(100ms);
      sub_qos.deadline(100ms);
      period = 200ms;
    }

    rclcpp::PublisherOptions popts;
    popts.event_callbacks.incompatible_qos_callback =
      [this](rclcpp::QOSOfferedIncompatibleQoSInfo &) {++pub_incompatible_;};
    popts.event_callbacks.deadline_callback =
      [this](rclcpp::QOSDeadlineOfferedInfo &) {++pub_deadline_missed_;};
    pub_ = create_publisher<Msg>("lab_topic", pub_qos, popts);

    sub_qos_ = sub_qos;
    if (late_sub) {
      Msg m;
      m.data = "map";
      pub_->publish(m);  // 只发一次，此时还没有订阅者
      late_timer_ = create_wall_timer(1s, [this]() {
          late_timer_->cancel();
          make_sub();
        });
    } else {
      make_sub();
      pub_timer_ = create_wall_timer(period, [this]() {
          Msg m;
          m.data = "data";
          pub_->publish(m);
        });
    }

    end_timer_ = create_wall_timer(3s, [this]() {
        RCLCPP_INFO(
          get_logger(),
          "[%s] received=%d | sub: incompatible=%d deadline_missed=%d | "
          "pub: incompatible=%d deadline_missed=%d",
          case_.c_str(), received_, sub_incompatible_, sub_deadline_missed_,
          pub_incompatible_, pub_deadline_missed_);
        rclcpp::shutdown();
      });
  }

private:
  void make_sub()
  {
    rclcpp::SubscriptionOptions sopts;
    sopts.event_callbacks.incompatible_qos_callback =
      [this](rclcpp::QOSRequestedIncompatibleQoSInfo &) {++sub_incompatible_;};
    sopts.event_callbacks.deadline_callback =
      [this](rclcpp::QOSDeadlineRequestedInfo &) {++sub_deadline_missed_;};
    sub_ = create_subscription<Msg>(
      "lab_topic", sub_qos_, [this](const Msg::ConstSharedPtr &) {++received_;}, sopts);
  }

  std::string case_;
  rclcpp::QoS sub_qos_{10};
  rclcpp::Publisher<Msg>::SharedPtr pub_;
  rclcpp::Subscription<Msg>::SharedPtr sub_;
  rclcpp::TimerBase::SharedPtr pub_timer_, late_timer_, end_timer_;
  int received_{0};
  int sub_incompatible_{0}, sub_deadline_missed_{0};
  int pub_incompatible_{0}, pub_deadline_missed_{0};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  std::string c = "be-pub-rel-sub";
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::strcmp(argv[i], "--case") == 0) {c = argv[i + 1];}
  }
  rclcpp::spin(std::make_shared<QosLab>(c));
  return 0;
}
