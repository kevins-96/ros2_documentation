# 答案：接口定义

> 先在[卡片](../cards/communication/interface-definition.md)里自己写下答案再来对照。

## 题 1：写一个 FollowWaypoints.action

```text
# goal
geometry_msgs/PoseStamped[] waypoints
float32 max_speed 0.5
---
# result
bool reached
float64 elapsed_s
---
# feedback
uint32 completed_waypoints
```

- `float32 max_speed 0.5`：基本类型可以有默认值。📄
- `geometry_msgs/PoseStamped[] waypoints`：可以作为字段类型使用，但**不能给它写默认值**，因为默认值不支持复合类型。📄
- 构建时 `rosidl_generate_interfaces(... DEPENDENCIES geometry_msgs)`，`package.xml` 加 `<depend>geometry_msgs</depend>`。📄

## 题 2：接口和节点在同一个包 vs 分开

**同一个包（`my_nav`）：** 除了 `rosidl_generate_interfaces`，节点目标还需要链接本包生成的类型支持：📄

```cmake
rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} rosidl_typesupport_cpp)
target_link_libraries(my_node "${cpp_typesupport_target}")
```

**分开（`my_nav_interfaces`）：** 接口包只做生成；`my_nav` 里 `find_package(my_nav_interfaces REQUIRED)`，`target_link_libraries(my_node ${my_nav_interfaces_TARGETS})`，`package.xml` 加 `<depend>my_nav_interfaces</depend>`。📄（Custom-ROS2-Interfaces 教程的做法）🔍 本仓库 `comm_lab` 用的就是这种方式。

**各自好处（💡）：**
- 同包：小项目文件少，改动集中。
- 分包：其他包（RViz 插件、Python 工具、别的机器人）只依赖接口包，不必拉进你的节点代码和依赖；接口变更也更显眼。工程上更常用分包。
