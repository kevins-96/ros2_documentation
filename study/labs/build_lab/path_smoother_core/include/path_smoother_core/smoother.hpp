#ifndef PATH_SMOOTHER_CORE__SMOOTHER_HPP_
#define PATH_SMOOTHER_CORE__SMOOTHER_HPP_

#include <memory>
#include <string>

#include "path_smoother_core/path_math.hpp"
#include "rclcpp/rclcpp.hpp"

namespace path_smoother_core
{

// 插件基类（Nav2 风格）：pluginlib 要求能默认构造，所以参数通过 configure() 传入
class Smoother
{
public:
  using Ptr = std::shared_ptr<Smoother>;
  virtual ~Smoother() = default;

  // name 是插件实例名，插件自己的参数用 "<name>.xxx" 声明，避免多个实例冲突
  virtual void configure(const rclcpp::Node::SharedPtr & node, const std::string & name) = 0;
  virtual Path2D smooth(const Path2D & path) = 0;

protected:
  Smoother() = default;
};

}  // namespace path_smoother_core

#endif  // PATH_SMOOTHER_CORE__SMOOTHER_HPP_
