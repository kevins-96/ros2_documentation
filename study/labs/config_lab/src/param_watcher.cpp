// 卡片「参数」：用 ParameterEventHandler 监听另一个节点（/planner）的参数变化
// 运行：先运行 card_parameter_min，再 ros2 run config_lab param_watcher
#include <memory>

#include "rclcpp/rclcpp.hpp"

class ParamWatcher : public rclcpp::Node
{
public:
  ParamWatcher()
  : Node("param_watcher")
  {
    handler_ = std::make_shared<rclcpp::ParameterEventHandler>(this);
    // 第三个参数是远程节点的全名；返回的句柄必须保存，否则回调立即失效
    handle_ = handler_->add_parameter_callback(
      "max_speed",
      [this](const rclcpp::Parameter & p) {
        RCLCPP_INFO(get_logger(), "/planner max_speed -> %.2f", p.as_double());
      },
      "/planner");
  }

private:
  std::shared_ptr<rclcpp::ParameterEventHandler> handler_;
  rclcpp::ParameterCallbackHandle::SharedPtr handle_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<ParamWatcher>());
  rclcpp::shutdown();
  return 0;
}
