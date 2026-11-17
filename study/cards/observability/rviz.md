---
tags: [ros2, rviz, rqt, marker, 可视化]
distro: rolling
status: batch-6
aliases: [RViz 与可视化, RViz, rviz2, RQt, rqt, Marker, MarkerArray, Fixed Frame, 固定坐标系, visualization_msgs, LINE_STRIP, 可视化调试]
---

# RViz 与可视化

> 适用：ROS 2 **Rolling**（2026-11-17；镜像里没有图形界面和 rviz2，RViz 操作部分以文档为准，消息字段和 tf2 行为在容器里实测）
> 来源标记：📄 仓库文档原文 · 🔍 源码或实测核实 · 💡 经验补充（可信，但请自己验证）
> 源文档：[About-visualization.rst](../../../source/Developer-Tools/About-visualization.rst)、[RViz-User-Guide.rst](../../../source/Developer-Tools/Visualization/RViz/RViz-User-Guide/RViz-User-Guide.rst)、[Marker-Display-types.rst](../../../source/Developer-Tools/Visualization/RViz/Marker-Display-types/Marker-Display-types.rst)、[Marker-Sending-Basic-Shapes.rst](../../../source/Developer-Tools/Visualization/RViz/Marker-Sending-Basic-Shapes/Marker-Sending-Basic-Shapes.rst)、[Marker-Points-and-Lines.rst](../../../source/Developer-Tools/Visualization/RViz/Marker-Points-and-Lines/Marker-Points-and-Lines.rst)、[About-RQt.rst](../../../source/Developer-Tools/Visualization/About-RQt.rst)

## 1. 一句话定义

RViz 把各种消息（地图、路径、点云、TF、Marker）**通过 tf2 变换到同一个固定坐标系（Fixed Frame）**里画成 3D 场景；Marker 是"没有现成消息类型能画"时自己描述形状的通用消息。RQt 是 Qt 插件框架，用来装各种 2D 工具（话题图、日志、参数、曲线）。📄

## 2. 为什么需要

💡 规划控制的很多 bug 只有画出来才看得出：路径穿墙、代价地图膨胀半径不对、局部目标点选错、坐标系差了一个旋转。把算法的**中间结果**（候选轨迹、碰撞点、前瞻点）用 Marker 发出来，比读日志快得多。

### 两个坐标系（📄）

| | 作用 | 设错了会怎样 |
|---|---|---|
| **Fixed Frame**（Global Options 里） | 所有数据变换到这个系里显示，应该相对世界静止：`map`、`odom`、`world` | 设成 `base_link`：机器人看到过的东西都"跟着机器人走"，出现在机器人前方固定位置 |
| **Target Frame**（Views 面板） | 相机跟随的坐标系 | 只影响视角：设成 `base_link` 时机器人在画面中央不动、世界在动 |

📄 改 Fixed Frame 会清空当前显示的数据，而不是重新变换。

### Display 的状态（📄）

每个 Display 有 `OK`、`Warning`、`Error`、`Disabled` 四种状态，标题背景色表示；展开 `Status` 能看到具体原因（比如 `No transform from [laser] to [map]`、`No messages received`）。💡 "什么都没显示"时第一步就是看这里，而不是去改代码。

### Marker 的关键字段（📄 + 🔍）

| 字段 | 含义 | 坑 |
|---|---|---|
| `header.frame_id` | 形状在哪个坐标系里 | 🔍 不能以 `/` 开头，见常见坑 2 |
| `ns` + `id` | 唯一标识；同一 `ns` + `id` 的新消息**替换**旧的 | 📄 唯一性由你负责；🔍 练习里路径线 `id=0`、终点球 `id=1` |
| `type` | `ARROW`、`CUBE`、`SPHERE`、`LINE_STRIP`、`LINE_LIST`、`POINTS`、`TEXT_VIEW_FACING`、`MESH_RESOURCE` 等 | `LINE_STRIP` 只用 `scale.x`（线宽） |
| `action` | 🔍 `ADD=0`（同时是修改，`MODIFY` 也是 0）、`DELETE=2`、`DELETEALL=3`；📄 1 已弃用 | |
| `color.a` | 不透明度，1 不透明，0 全透明 | 🔍 **默认 0**，见常见坑 1 |
| `lifetime` | 多久没更新就自动删除；0 表示永久 | 📄 同一 `ns`/`id` 收到新消息时倒计时重置 |
| `frame_locked` | 是否每帧按最新 TF 重新变换 | 📄 跟着机器人动的标记设 true |
| `colors` | 每个顶点一个颜色（`POINTS`、`LINE_*`） | 🔍 `Marker.msg` 注释：`alpha is not yet used` |

## 3. 最小 C++ 代码

完整代码：[fake_planner.cpp](../../labs/observe_lab/src/fake_planner.cpp) 的 `on_timer()`

```cpp
#include "visualization_msgs/msg/marker_array.hpp"

visualization_msgs::msg::MarkerArray arr;

visualization_msgs::msg::Marker line;
line.header = path.header;                       // frame_id "map"，不要写 "/map"
line.ns = "plan";
line.id = 0;
line.type = visualization_msgs::msg::Marker::LINE_STRIP;
line.action = visualization_msgs::msg::Marker::ADD;
line.pose.orientation.w = 1.0;                   // 四元数默认全 0，不是合法旋转
line.scale.x = 0.05;                             // 线宽
line.color.g = 1.0;
line.color.a = 1.0;                              // 默认 0 = 全透明
for (const auto & p : path.poses) {
  line.points.push_back(p.pose.position);
}
arr.markers.push_back(line);

visualization_msgs::msg::Marker goal;
goal.header = path.header;
goal.ns = "plan";
goal.id = 1;                                     // 同一 ns 下不能和 line 重复
goal.type = visualization_msgs::msg::Marker::SPHERE;
goal.pose.position = path.poses.back().pose.position;
goal.pose.orientation.w = 1.0;
goal.scale.x = goal.scale.y = goal.scale.z = 0.3;
goal.color.r = 1.0;
goal.color.a = 1.0;
goal.lifetime = rclcpp::Duration(2s);            // 2 秒没更新就自动消失
arr.markers.push_back(goal);

marker_pub_->publish(arr);                       // 一次发一组，减少消息数
```

RViz 里：Add → By topic → `/plan_markers`（MarkerArray），Fixed Frame 设 `map`。

### RQt（📄）

`rqt` 打开统一窗口，从 Plugins 菜单装插件；也可以单独运行某个插件（例如 `rqt_graph`、`rqt_console`、`rqt_plot`）。`ros2 pkg list | grep rqt` 看装了哪些。💡 规划控制常用：`rqt_graph`（节点和话题连线）、`rqt_console`（日志过滤，见[日志](../configuration/logging.md)）、`rqt_reconfigure`（运行时调参数）、`rqt_plot` 或 PlotJuggler（画曲线；📄 About-Debugging 里列为社区工具，Foxglove、Rerun 也在其中）。

## 4. 常见坑

1. **忘了设 alpha，Marker 看不见。** 🔍 `visualization_msgs::msg::Marker` 默认构造后 `color` 是 `(0, 0, 0, 0)`，全透明；`fake_planner --ros-args -p marker_alpha:=0.0` 发出的 Marker 消息本身完全正常（`ros2 topic echo` 能看到），RViz 里却什么都没有，Display 状态也可能是 OK。📄 文档示例里专门写了 `// Don't forget to set the alpha!`。
2. **`frame_id` 以 `/` 开头。** 📄 `Marker-Display-types.rst` 的示例写的是 `"/my_frame"`，`Marker-Sending-Basic-Shapes.rst` 已经指出 ROS 2 的坐标系名不以 `/` 开头。🔍 tf2 直接拒绝：`Invalid argument "/map" passed to lookupTransform argument target_frame - in tf2 frame_ids cannot start with a '/'`；而发布端没有任何检查，`frame_id: /map` 的 Marker 照样发出去。💡 从 ROS 1 移植的代码最常见。
3. **`pose.orientation` 全 0。** 💡 消息默认四元数是 `(0,0,0,0)`，不是单位四元数；RViz 会对它报警或画不出来（见[四元数](../coordinates/quaternion.md)）。即使只用 `points`，也把 `pose.orientation.w = 1.0` 写上。
4. **`ns`/`id` 冲突导致形状"闪"或互相覆盖。** 📄 同一 `ns` + `id` 是同一个对象。💡 多条候选轨迹用同一个 `id` 发，只会看到最后一条；用循环下标做 `id`，并在条数变少时发 `DELETEALL` 或给每个设 `lifetime`，否则旧的会一直留着。
5. **Fixed Frame 设成会动的坐标系。** 📄 设成 `base_link` 时历史数据跟着机器人走。💡 "地图在转"、"激光点拖尾方向不对"通常就是这个原因。
6. **没有 TF 时 Display 是 Error 而不是空白。** 📄 Fixed Frame 设成数据里不存在的坐标系，或者缺变换时，Display 的 Status 会写原因。💡 调 Marker 时没有 TF 树，就把 Fixed Frame 直接设成 Marker 的 `frame_id`（文档教程也是这样做的）。
7. **大 MarkerArray 拖慢 RViz。** 💡 每条候选轨迹几百个点、每秒十几次，RViz 很快就卡；降频发布、只发前 N 条、或者用 `POINTS` 代替一堆小 `SPHERE`。

## 5. 与其他概念的关系

- [tf2 坐标变换](../coordinates/tf2.md)：RViz 的一切显示都经过 tf2；frame 名规则相同
- [四元数](../coordinates/quaternion.md)：Marker 的 `pose.orientation`
- [URDF 与机器人模型](../coordinates/urdf.md)：RobotModel Display
- [QoS](../communication/qos.md)：地图是 transient local；RViz 的 Map Display 订阅 QoS 要对上
- [日志](../configuration/logging.md)：`rqt_console`
- [rosbag 录制回放](rosbag.md)：回放 bag 时用 RViz 看，记得 `use_sim_time`
- 练习：[录制、回放与离线分析](../../labs/bag-record-replay.md)（第 4 部分的 Marker 检查）
- 上一级：[总览脑图](../../map/overview.md)

## 6. 自测题（先自己答，再看[答案](../../answers/rviz-answers.md)）

1. 你给局部规划器加了一个 MarkerArray 显示 5 条候选轨迹，RViz 里只看到 1 条，而且偶尔一闪就没了。列出至少三个可能原因，以及各自怎么在不改代码的情况下确认（提示：`ros2 topic echo --field`）。
2. 用 RViz 看机器人回放 bag：地图正常，激光点云在机器人转弯时会"拖出弧线"、直行时正常。Fixed Frame 是 `base_link`。解释原因并说出改法。
