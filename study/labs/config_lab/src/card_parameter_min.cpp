// 卡片「参数」最小代码：声明 + 描述符 + 三种参数回调
// 运行：ros2 run config_lab card_parameter_min
//       ros2 run config_lab card_parameter_min --ros-args --params-file <包>/config/planner.yaml
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "rcl_interfaces/msg/parameter_descriptor.hpp"
#include "rcl_interfaces/msg/set_parameters_result.hpp"
#include "rclcpp/rclcpp.hpp"

class PlannerParams : public rclcpp::Node
{
public:
  PlannerParams()
  : Node("planner")
  {
    // 1) 带范围的 double 参数
    rcl_interfaces::msg::ParameterDescriptor speed_desc;
    speed_desc.description = "最大线速度 m/s";
    rcl_interfaces::msg::FloatingPointRange range;
    range.from_value = 0.0;
    range.to_value = 2.0;
    speed_desc.floating_point_range = {range};
    max_speed_ = declare_parameter("max_speed", 0.5, speed_desc);
    max_accel_ = declare_parameter("max_accel", 1.0);

    // 2) 只读参数：只能在启动时给值
    rcl_interfaces::msg::ParameterDescriptor ro;
    ro.read_only = true;
    robot_id_ = declare_parameter("robot_id", "bot_0", ro);

    // 3) 用点号分组：YAML 里写成嵌套的 controller: {kp: ..., ki: ...}
    kp_ = declare_parameter("controller.kp", 1.0);
    declare_parameter("controller.ki", 0.0);
    declare_parameter("frame_ids", std::vector<std::string>{"map", "odom"});

    // pre-set：可以改写"即将设置"的列表。这里：改 max_speed 时自动把 max_accel 设为它的 2 倍
    pre_handle_ = add_pre_set_parameters_callback(
      [](std::vector<rclcpp::Parameter> & params) {
        for (const auto & p : std::vector<rclcpp::Parameter>(params)) {
          if (p.get_name() == "max_speed" && p.get_type() == rclcpp::ParameterType::PARAMETER_DOUBLE) {
            params.emplace_back("max_accel", 2.0 * p.as_double());
          }
        }
      });

    // set：只做检查，决定接受还是拒绝。不要在这里改成员变量（后面的回调可能还会拒绝）
    set_handle_ = add_on_set_parameters_callback(
      [](const std::vector<rclcpp::Parameter> & params) {
        rcl_interfaces::msg::SetParametersResult r;
        r.successful = true;
        for (const auto & p : params) {
          if (p.get_name() == "controller.kp" && p.as_double() < 0.0) {
            r.successful = false;
            r.reason = "controller.kp must be >= 0";
          }
        }
        return r;
      });

    // post：参数已经生效，在这里更新成员变量
    post_handle_ = add_post_set_parameters_callback(
      [this](const std::vector<rclcpp::Parameter> & params) {
        for (const auto & p : params) {
          if (p.get_name() == "max_speed") {max_speed_ = p.as_double();}
          if (p.get_name() == "max_accel") {max_accel_ = p.as_double();}
          if (p.get_name() == "controller.kp") {kp_ = p.as_double();}
          RCLCPP_INFO(get_logger(), "applied %s = %s",
            p.get_name().c_str(), p.value_to_string().c_str());
        }
      });

    RCLCPP_INFO(get_logger(), "robot_id=%s max_speed=%.2f max_accel=%.2f kp=%.2f use_sim_time=%s",
      robot_id_.c_str(), max_speed_, max_accel_, kp_,
      get_parameter("use_sim_time").as_bool() ? "true" : "false");
  }

private:
  double max_speed_, max_accel_, kp_;
  std::string robot_id_;
  // 句柄必须保存，析构即注销回调
  rclcpp::node_interfaces::PreSetParametersCallbackHandle::SharedPtr pre_handle_;
  rclcpp::node_interfaces::OnSetParametersCallbackHandle::SharedPtr set_handle_;
  rclcpp::node_interfaces::PostSetParametersCallbackHandle::SharedPtr post_handle_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<PlannerParams>());
  rclcpp::shutdown();
  return 0;
}
