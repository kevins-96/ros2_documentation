#ifndef MOVING_AVERAGE_SMOOTHER_HPP_
#define MOVING_AVERAGE_SMOOTHER_HPP_

#include <cstddef>
#include <string>

#include "path_smoother_core/smoother.hpp"

namespace path_smoother_plugins
{

class MovingAverageSmoother : public path_smoother_core::Smoother
{
public:
  void configure(const rclcpp::Node::SharedPtr & node, const std::string & name) override;
  path_smoother_core::Path2D smooth(const path_smoother_core::Path2D & path) override;
  std::size_t window() const {return window_;}

private:
  std::size_t window_{3};
};

}  // namespace path_smoother_plugins

#endif  // MOVING_AVERAGE_SMOOTHER_HPP_
