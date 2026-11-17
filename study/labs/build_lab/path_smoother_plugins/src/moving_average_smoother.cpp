#include "moving_average_smoother.hpp"

#include <stdexcept>

#include "pluginlib/class_list_macros.hpp"

namespace path_smoother_plugins
{

void MovingAverageSmoother::configure(
  const rclcpp::Node::SharedPtr & node, const std::string & name)
{
  // 参数名带实例名前缀：smoother.window
  const auto w = node->declare_parameter<int>(name + ".window", 3);
  if (w <= 0 || w % 2 == 0) {
    throw std::invalid_argument(name + ".window must be a positive odd number");
  }
  window_ = static_cast<std::size_t>(w);
  RCLCPP_INFO(node->get_logger(), "MovingAverageSmoother '%s': window=%zu", name.c_str(), window_);
}

path_smoother_core::Path2D MovingAverageSmoother::smooth(const path_smoother_core::Path2D & path)
{
  return path_smoother_core::moving_average(path, window_);
}

}  // namespace path_smoother_plugins

// 第一个参数：派生类；第二个参数：基类。都写完整命名空间
PLUGINLIB_EXPORT_CLASS(path_smoother_plugins::MovingAverageSmoother, path_smoother_core::Smoother)
