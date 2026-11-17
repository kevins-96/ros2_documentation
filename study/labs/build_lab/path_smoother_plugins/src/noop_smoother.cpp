#include <string>

#include "path_smoother_core/smoother.hpp"
#include "pluginlib/class_list_macros.hpp"

namespace path_smoother_plugins
{

class NoopSmoother : public path_smoother_core::Smoother
{
public:
  void configure(const rclcpp::Node::SharedPtr & node, const std::string & name) override
  {
    RCLCPP_INFO(node->get_logger(), "NoopSmoother '%s' configured", name.c_str());
  }
  path_smoother_core::Path2D smooth(const path_smoother_core::Path2D & path) override
  {
    return path;
  }
};

}  // namespace path_smoother_plugins

PLUGINLIB_EXPORT_CLASS(path_smoother_plugins::NoopSmoother, path_smoother_core::Smoother)
