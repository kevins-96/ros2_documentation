// 第 9 批：放进组件容器的小节点，打印自己的全名、话题 scan 解析成什么、参数 tag。
// 用来核对 150_roslaunch 的"容器的命令行参数不能作用到动态加载的节点上"。
#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_components/register_node_macro.hpp"

namespace design_lab
{
class NameProbe : public rclcpp::Node
{
public:
  explicit NameProbe(const rclcpp::NodeOptions & options)
  : Node("name_probe", options)
  {
    auto tag = declare_parameter("tag", std::string("<未设置>"));
    RCLCPP_INFO(get_logger(), "全名=%s scan->%s tag=%s", get_fully_qualified_name(),
      get_node_topics_interface()->resolve_topic_name("scan").c_str(), tag.c_str());
  }
};
}  // namespace design_lab

RCLCPP_COMPONENTS_REGISTER_NODE(design_lab::NameProbe)
