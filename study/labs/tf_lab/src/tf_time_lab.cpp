// 练习「lookupTransform 的时间参数」：同一个查询 odom <- laser，换不同的时间参数，统计成功/失败。
// 先在另一个终端运行 card_tf2_min（20 Hz 发布 odom -> base_link，静态 base_link -> laser），
// 再运行：ros2 run tf_lab tf_time_lab --mode <模式>
//   --mode latest          time = tf2::TimePointZero（最新可用）
//   --mode now             time = now()，不等待
//   --mode now-timeout     time = now()，timeout 50 ms
//   --mode past-5s         time = now() - 5 s，timeout 50 ms
//   --mode past-15s        time = now() - 15 s，timeout 50 ms（buffer 默认只存 10 s）
//   --mode self-broadcast  本节点自己 20 Hz 广播 odom -> self，在同一个单线程执行器里
//                          用 now() + 50 ms timeout 查 odom <- self
// 每 100 ms 查一次，跑 --seconds 秒（默认 8）后打印汇总。
#include <chrono>
#include <cstring>
#include <map>
#include <memory>
#include <string>

#include "geometry_msgs/msg/transform_stamped.hpp"
#include "rclcpp/rclcpp.hpp"
#include "tf2/exceptions.hpp"
#include "tf2_ros/buffer.hpp"
#include "tf2_ros/transform_broadcaster.hpp"
#include "tf2_ros/transform_listener.hpp"

using namespace std::chrono_literals;

class TimeLab : public rclcpp::Node
{
public:
  TimeLab(std::string mode, double seconds)
  : Node("tf_time_lab"), mode_(std::move(mode))
  {
    buffer_ = std::make_unique<tf2_ros::Buffer>(get_clock());
    listener_ = std::make_shared<tf2_ros::TransformListener>(*buffer_, *this);
    if (mode_ == "self-broadcast") {
      child_ = "self";
      broadcaster_ = std::make_unique<tf2_ros::TransformBroadcaster>(*this);
      pub_timer_ = create_wall_timer(50ms, [this]() {
          geometry_msgs::msg::TransformStamped t;
          t.header.stamp = now();
          t.header.frame_id = "odom";
          t.child_frame_id = "self";
          t.transform.rotation.w = 1.0;
          broadcaster_->sendTransform(t);
        });
    }
    start_ = now();
    query_timer_ = create_wall_timer(100ms, [this]() {query();});
    stop_timer_ = create_wall_timer(
      std::chrono::duration<double>(seconds), [this]() {report(); rclcpp::shutdown();});
  }

private:
  void query()
  {
    const rclcpp::Time t_now = now();
    const double elapsed = (t_now - start_).seconds();
    try {
      geometry_msgs::msg::TransformStamped t;
      if (mode_ == "latest") {
        t = buffer_->lookupTransform("odom", child_, tf2::TimePointZero);
      } else if (mode_ == "now") {
        t = buffer_->lookupTransform("odom", child_, t_now);
      } else if (mode_ == "past-5s") {
        t = buffer_->lookupTransform("odom", child_, t_now - rclcpp::Duration(5s), 50ms);
      } else if (mode_ == "past-15s") {
        t = buffer_->lookupTransform("odom", child_, t_now - rclcpp::Duration(15s), 50ms);
      } else {  // now-timeout, self-broadcast
        t = buffer_->lookupTransform("odom", child_, t_now, 50ms);
      }
      ++ok_;
      if (first_ok_ < 0) {first_ok_ = elapsed;}
      age_sum_ms_ += (t_now - rclcpp::Time(t.header.stamp)).seconds() * 1e3;
    } catch (const tf2::TransformException & ex) {
      ++fail_;
      std::string kind = dynamic_cast<const tf2::ExtrapolationException *>(&ex) ? "Extrapolation" :
        dynamic_cast<const tf2::LookupException *>(&ex) ? "Lookup" :
        dynamic_cast<const tf2::ConnectivityException *>(&ex) ? "Connectivity" :
        dynamic_cast<const tf2::TimeoutException *>(&ex) ? "Timeout" : "Other";
      if (first_err_.count(kind) == 0) {first_err_[kind] = ex.what();}
      ++err_count_[kind];
    }
    const double cost_ms = (now() - t_now).seconds() * 1e3;
    if (cost_ms > max_block_ms_) {max_block_ms_ = cost_ms;}
  }

  void report()
  {
    std::printf("\n=== mode=%s  ok=%d  fail=%d", mode_.c_str(), ok_, fail_);
    if (ok_ > 0) {
      std::printf("  first_ok_at=%.1fs  avg_age_of_result=%.1fms", first_ok_, age_sum_ms_ / ok_);
    }
    std::printf("  max_query_block=%.1fms\n", max_block_ms_);
    for (auto & [kind, msg] : first_err_) {
      std::printf("  [%s x%d] %s\n", kind.c_str(), err_count_[kind], msg.c_str());
    }
    std::fflush(stdout);
  }

  std::string mode_;
  std::string child_ = "laser";
  std::unique_ptr<tf2_ros::Buffer> buffer_;
  std::shared_ptr<tf2_ros::TransformListener> listener_;
  std::unique_ptr<tf2_ros::TransformBroadcaster> broadcaster_;
  rclcpp::TimerBase::SharedPtr pub_timer_, query_timer_, stop_timer_;
  rclcpp::Time start_;
  int ok_ = 0, fail_ = 0;
  double first_ok_ = -1.0, age_sum_ms_ = 0.0, max_block_ms_ = 0.0;
  std::map<std::string, std::string> first_err_;
  std::map<std::string, int> err_count_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  std::string mode = "latest";
  double seconds = 8.0;
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::strcmp(argv[i], "--mode") == 0) {mode = argv[i + 1];}
    if (std::strcmp(argv[i], "--seconds") == 0) {seconds = std::stod(argv[i + 1]);}
  }
  rclcpp::spin(std::make_shared<TimeLab>(mode, seconds));   // 单线程执行器
  return 0;
}
