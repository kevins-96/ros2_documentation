// 按参数 smoother_plugin 的名字用 pluginlib 加载平滑器，对一条锯齿路径做平滑并发布
#include <cmath>
#include <memory>
#include <string>

#include "geometry_msgs/msg/pose_stamped.hpp"
#include "nav_msgs/msg/path.hpp"
#include "pluginlib/class_loader.hpp"
#include "rclcpp/rclcpp.hpp"

#include "path_smoother_core/path_math.hpp"
#include "path_smoother_core/smoother.hpp"

using path_smoother_core::Path2D;

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<rclcpp::Node>("smoother_node");
  const auto plugin = node->declare_parameter<std::string>("smoother_plugin", "moving_average");
  const auto points = node->declare_parameter<int>("points", 11);

  // 第一个参数：基类所在的包；第二个参数：基类的完整类名
  // （要和 plugins.xml 里的 base_class_type 一致）
  pluginlib::ClassLoader<path_smoother_core::Smoother> loader(
    "path_smoother_core", "path_smoother_core::Smoother");
  for (const auto & name : loader.getDeclaredClasses()) {
    RCLCPP_INFO(node->get_logger(), "declared plugin: %s (type %s)",
      name.c_str(), loader.getClassType(name).c_str());
  }

  path_smoother_core::Smoother::Ptr smoother;
  try {
    smoother = loader.createSharedInstance(plugin);
  } catch (const pluginlib::PluginlibException & e) {
    RCLCPP_FATAL(node->get_logger(), "cannot load '%s': %s", plugin.c_str(), e.what());
    rclcpp::shutdown();
    return 1;
  }
  smoother->configure(node, "smoother");

  // 锯齿路径：x 每步 +1，y 在 0 和 1 之间跳
  Path2D raw;
  for (int i = 0; i < points; ++i) {
    raw.push_back({static_cast<double>(i), (i % 2) ? 1.0 : 0.0});
  }
  const Path2D out = smoother->smooth(raw);
  RCLCPP_INFO(node->get_logger(),
    "plugin=%s points=%zu length %.3f -> %.3f, max turn %.1f -> %.1f deg",
    plugin.c_str(), out.size(),
    path_smoother_core::path_length(raw), path_smoother_core::path_length(out),
    path_smoother_core::max_turn_angle(raw) * 180.0 / M_PI,
    path_smoother_core::max_turn_angle(out) * 180.0 / M_PI);

  // transient_local：后启动的订阅者（比如 launch 测试）也能收到这一条
  auto pub = node->create_publisher<nav_msgs::msg::Path>(
    "smoothed_path", rclcpp::QoS(1).transient_local());
  nav_msgs::msg::Path msg;
  msg.header.frame_id = "map";
  msg.header.stamp = node->now();
  for (const auto & p : out) {
    geometry_msgs::msg::PoseStamped ps;
    ps.header = msg.header;
    ps.pose.position.x = p.x;
    ps.pose.position.y = p.y;
    ps.pose.orientation.w = 1.0;
    msg.poses.push_back(ps);
  }
  pub->publish(msg);

  rclcpp::spin(node);
  // 先释放插件实例，再让 loader 析构：否则插件库已卸载，析构函数无处可调
  smoother.reset();
  rclcpp::shutdown();
  return 0;
}
