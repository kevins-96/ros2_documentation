// 一个"假规划器"：给命令行工具、rosbag、服务内省、gdb、LTTng 和 RViz 提供观察对象
//   /map           nav_msgs/OccupancyGrid   transient local，启动时只发一次（像 map_server）
//   /plan          nav_msgs/Path            每 1/rate_hz 秒发一次（锯齿路径，终点跟随 /goal_pose）
//   /plan_markers  visualization_msgs/MarkerArray  路径线 + 终点球
//   /goal_pose     geometry_msgs/PoseStamped（订阅）
//   /replan        std_srvs/srv/Trigger（服务，可开启内省）
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include "geometry_msgs/msg/pose_stamped.hpp"
#include "nav_msgs/msg/occupancy_grid.hpp"
#include "nav_msgs/msg/path.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_srvs/srv/trigger.hpp"
#include "visualization_msgs/msg/marker_array.hpp"

using namespace std::chrono_literals;

namespace
{
// 练习「调试与回溯」：三层调用，最里面一层解引用空指针
struct Plan
{
  int waypoints{0};
};

int count_waypoints(const Plan * plan)
{
  return plan->waypoints;   // plan 为 nullptr 时在这里崩溃
}

int validate_plan(const Plan * plan)
{
  return count_waypoints(plan) + 1;
}

void busy_wait(std::chrono::microseconds d)
{
  const auto end = std::chrono::steady_clock::now() + d;
  while (std::chrono::steady_clock::now() < end) {
  }
}
}  // namespace

class FakePlanner : public rclcpp::Node
{
public:
  FakePlanner()
  : Node("fake_planner")
  {
    const double rate_hz = declare_parameter("rate_hz", 2.0);
    points_ = declare_parameter("points", 20);
    work_ms_ = declare_parameter("work_ms", 0.0);         // tracing 练习：每次规划额外"算"多久
    crash_on_replan_ = declare_parameter("crash_on_replan", false);   // 调试练习
    marker_alpha_ = declare_parameter("marker_alpha", 1.0);  // RViz 练习：0 时 Marker 看不见
    frame_id_ = declare_parameter("frame_id", std::string("map"));
    const auto introspection = declare_parameter("service_introspection", std::string("disabled"));

    // 地图：transient local + depth 1，后启动的订阅者（RViz、rosbag）也能收到
    map_pub_ = create_publisher<nav_msgs::msg::OccupancyGrid>(
      "map", rclcpp::QoS(1).transient_local().reliable());
    publish_map();

    plan_pub_ = create_publisher<nav_msgs::msg::Path>("plan", 10);
    marker_pub_ = create_publisher<visualization_msgs::msg::MarkerArray>("plan_markers", 10);
    goal_sub_ = create_subscription<geometry_msgs::msg::PoseStamped>(
      "goal_pose", 10, [this](geometry_msgs::msg::PoseStamped::ConstSharedPtr msg) {
        goal_x_ = msg->pose.position.x;
        goal_y_ = msg->pose.position.y;
        RCLCPP_INFO(get_logger(), "new goal (%.1f, %.1f)", goal_x_, goal_y_);
      });

    replan_srv_ = create_service<std_srvs::srv::Trigger>(
      "replan",
      [this](const std_srvs::srv::Trigger::Request::SharedPtr,
      std_srvs::srv::Trigger::Response::SharedPtr res) {
        ++replans_;
        if (crash_on_replan_) {
          const Plan * plan = nullptr;     // 模拟"规划失败返回了空指针却没检查"
          res->success = validate_plan(plan) > 0;
        } else {
          res->success = true;
        }
        res->message = "replanned " + std::to_string(replans_) + " times";
      });
    configure_introspection(introspection);

    timer_ = create_wall_timer(
      std::chrono::duration<double>(1.0 / rate_hz), [this]() {on_timer();});
    RCLCPP_INFO(get_logger(), "fake_planner up: rate %.1f Hz, %ld points, introspection %s",
      rate_hz, points_, introspection.c_str());
  }

private:
  void configure_introspection(const std::string & mode)
  {
    rcl_service_introspection_state_t state = RCL_SERVICE_INTROSPECTION_OFF;
    if (mode == "metadata") {
      state = RCL_SERVICE_INTROSPECTION_METADATA;
    } else if (mode == "contents") {
      state = RCL_SERVICE_INTROSPECTION_CONTENTS;
    }
    // 事件发布到隐藏话题 /replan/_service_event
    replan_srv_->configure_introspection(get_clock(), rclcpp::SystemDefaultsQoS(), state);
  }

  void publish_map()
  {
    nav_msgs::msg::OccupancyGrid map;
    map.header.frame_id = frame_id_;
    map.header.stamp = now();
    map.info.resolution = 0.5;
    map.info.width = 20;
    map.info.height = 20;
    map.info.origin.orientation.w = 1.0;
    map.data.assign(map.info.width * map.info.height, 0);
    for (uint32_t i = 5; i < 15; ++i) {
      map.data[10 * map.info.width + i] = 100;   // 中间一堵墙
    }
    map_pub_->publish(map);
  }

  void on_timer()
  {
    if (work_ms_ > 0.0) {
      busy_wait(std::chrono::microseconds(static_cast<int64_t>(work_ms_ * 1000.0)));
    }
    nav_msgs::msg::Path path;
    path.header.frame_id = frame_id_;
    path.header.stamp = now();
    for (int64_t i = 0; i < points_; ++i) {
      const double s = static_cast<double>(i) / static_cast<double>(points_ - 1);
      geometry_msgs::msg::PoseStamped p;
      p.header = path.header;
      p.pose.position.x = s * goal_x_;
      p.pose.position.y = s * goal_y_ + ((i % 2) ? 0.2 : -0.2);
      p.pose.orientation.w = 1.0;
      path.poses.push_back(p);
    }
    plan_pub_->publish(path);

    visualization_msgs::msg::MarkerArray arr;
    visualization_msgs::msg::Marker line;
    line.header = path.header;
    line.ns = "plan";
    line.id = 0;
    line.type = visualization_msgs::msg::Marker::LINE_STRIP;
    line.action = visualization_msgs::msg::Marker::ADD;
    line.pose.orientation.w = 1.0;
    line.scale.x = 0.05;                 // LINE_STRIP 只用 scale.x（线宽）
    line.color.g = 1.0;
    line.color.a = static_cast<float>(marker_alpha_);
    for (const auto & p : path.poses) {
      line.points.push_back(p.pose.position);
    }
    arr.markers.push_back(line);

    visualization_msgs::msg::Marker goal;
    goal.header = path.header;
    goal.ns = "plan";
    goal.id = 1;                          // 同一 ns 下 id 不能和路径线重复
    goal.type = visualization_msgs::msg::Marker::SPHERE;
    goal.action = visualization_msgs::msg::Marker::ADD;
    goal.pose.position = path.poses.back().pose.position;
    goal.pose.orientation.w = 1.0;
    goal.scale.x = goal.scale.y = goal.scale.z = 0.3;
    goal.color.r = 1.0;
    goal.color.a = static_cast<float>(marker_alpha_);
    goal.lifetime = rclcpp::Duration(2s);   // 2 秒没更新就自动消失
    arr.markers.push_back(goal);
    marker_pub_->publish(arr);
  }

  int64_t points_;
  double work_ms_;
  bool crash_on_replan_;
  double marker_alpha_;
  std::string frame_id_;
  double goal_x_{5.0};
  double goal_y_{2.0};
  int replans_{0};
  rclcpp::Publisher<nav_msgs::msg::OccupancyGrid>::SharedPtr map_pub_;
  rclcpp::Publisher<nav_msgs::msg::Path>::SharedPtr plan_pub_;
  rclcpp::Publisher<visualization_msgs::msg::MarkerArray>::SharedPtr marker_pub_;
  rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr goal_sub_;
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr replan_srv_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<FakePlanner>());
  rclcpp::shutdown();
  return 0;
}
