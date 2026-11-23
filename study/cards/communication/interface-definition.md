---
tags: [ros2, 通信, 接口, rosidl]
distro: rolling
status: batch-1
aliases: [接口定义, msg, srv, action, 自定义消息, rosidl]
---

# 接口定义 msg / srv / action

> 适用：ROS 2 **Rolling**（2026-11-17 在 `ros:rolling-ros-base` 上编译实测）
> 来源标记：📄 仓库文档原文 · 🔍 rclcpp 源码/实测核实 · 💡 经验补充（可信，但请自己验证）
> 源文档：[Interface-specifications.rst](../../../source/ROS-Framework/interfaces/Working-with-interfaces/Interface-specifications.rst)、[Custom-ROS2-Interfaces.rst](../../../source/ROS-Framework/client-libraries/Working-with-Client-Libraries/Custom-ROS2-Interfaces.rst)、[Single-Package-Define-And-Use-Interface.rst](../../../source/ROS-Framework/interfaces/Working-with-interfaces/Single-Package-Define-And-Use-Interface.rst)、[Creating-an-Action.rst](../../../source/ROS-Framework/interfaces/actions/Working-with-actions/Creating-an-Action.rst)
> 📐 设计动机：[接口定义的设计](../../design/interface-design.md)：`.msg` 为什么先转成 `.idl`、生成的 C++ 结构体为什么不提供位置参数构造函数、`MessageInitialization` 各模式的代价；[rmw 接口与序列化的设计](../../design/rmw-serialization-design.md)：`@optional` 能写但没效果、不支持字典、CDR 的字节数

## 1. 一句话定义

接口定义是用 ROS 的简化接口定义语言（IDL）写的 `.msg`、`.srv`、`.action` 文本文件，构建时由 `rosidl` 工具链生成 C++、Python 等语言的数据类型。📄

## 2. 为什么需要

- 节点之间要对"数据长什么样"达成一致，跨语言也能互通。📄
- 标准接口包（`std_msgs`、`geometry_msgs`、`nav_msgs`、`sensor_msgs`……）覆盖大多数需求；自定义接口用于你自己的领域数据，例如规划状态、任务描述。💡 先找有没有现成的，能用标准类型就别自造，工具（RViz、rosbag 插件）对标准类型支持最好。

### 三种文件 📄

| 文件 | 位置 | 结构 |
|---|---|---|
| `.msg` | 包的 `msg/` 目录 | 字段 + 常量 |
| `.srv` | `srv/` | 请求 `---` 响应（任意两个 msg 用 `---` 连起来就是合法 srv） |
| `.action` | `action/` | 目标 `---` 结果 `---` 反馈（各段可以有 0 个或多个字段） |

服务里不能嵌套服务。📄

### 字段语法 📄

```text
# 类型 名字 [默认值]
int32 my_int
string full_name "John Doe"       # 字符串默认值必须加引号
int32[] samples [-200, 0, 200]    # 数组默认值

# 常量：名字必须大写，用 =
uint8 STATUS_OK=0

# 数组与有界类型
int32[]        unbounded          # C++: std::vector
int32[5]       five               # C++: std::array<T, 5>
int32[<=5]     up_to_five         # C++: 有界向量 BoundedVector
string<=10     short_name         # 有界字符串
```

- 内置类型：`bool byte char float32 float64 int8…int64 uint8…uint64 string wstring`。C++ 映射：`float64` → `double`，`string` → `std::string`，`byte` → `uint8_t`。📄
- 字段名：小写字母、数字、下划线，字母开头，不能以下划线结尾，不能有连续下划线。📄
- 默认值目前**不支持**字符串数组和复合类型（嵌套消息）。📄
- 引用同一个包里的消息时**不写包名**；引用别的包写 `pkg/Type`。📄

## 3. 最小代码

本仓库的接口包 [study_interfaces](../../labs/study_interfaces/) 就是一个完整例子。

`msg/PathStatus.msg`：

```text
uint8 STATUS_OK=0
uint8 STATUS_NO_PATH=1
uint8 STATUS_TIMEOUT=2

std_msgs/Header header
uint8 status 0
float32 length_m 0.0
Pose2D[<=1000] waypoints     # 同包里的 Pose2D，不写包名
```

`CMakeLists.txt`（接口包**只能**是 ament_cmake 包 📄）：

```cmake
find_package(rosidl_default_generators REQUIRED)
find_package(std_msgs REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}   # 第一个参数必须以包名开头
  "msg/Pose2D.msg"
  "msg/PathStatus.msg"
  "srv/ComputePath.srv"
  "action/MoveTo.action"
  DEPENDENCIES std_msgs                      # 字段里用到的其他接口包
)
ament_export_dependencies(rosidl_default_runtime)
```

`package.xml`（📄 三行缺一不可）：

```xml
<buildtool_depend>rosidl_default_generators</buildtool_depend>
<exec_depend>rosidl_default_runtime</exec_depend>
<member_of_group>rosidl_interface_packages</member_of_group>
<depend>std_msgs</depend>
```

在 C++ 里使用（[card_interface_min.cpp](../../labs/comm_lab/src/card_interface_min.cpp)）：

```cpp
#include "study_interfaces/msg/path_status.hpp"   // PathStatus -> path_status.hpp

study_interfaces::msg::PathStatus msg;               // 字段已有默认值
msg.status = study_interfaces::msg::PathStatus::STATUS_NO_PATH;  // 常量是静态成员
msg.waypoints.resize(2);                              // 有界数组，最多 1000 个
```

使用方的 CMake：`target_link_libraries(my_node ${study_interfaces_TARGETS})`。🔍 **在接口包自己里面**使用自己的消息时，要用 `rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} rosidl_typesupport_cpp)` 再链接 `${cpp_typesupport_target}`。📄

## 4. 常见坑

1. **Rolling 的 `geometry_msgs` 里已经没有 `Pose2D`。** 🔍 本仓库第一版接口引用了 `geometry_msgs/Pose2D`，在 `ros:rolling-ros-base` 里构建报错 `No such file or directory: .../geometry_msgs/msg/Pose2D.json`。旧教程、旧代码里常见这个类型；Rolling 上请自己定义或改用 `geometry_msgs/Pose`。
2. **`rosidl_generate_interfaces` 第一个参数没以包名开头。** 📄 会出问题（rosidl issue #441）。直接写 `${PROJECT_NAME}`。
3. **漏了 `<member_of_group>rosidl_interface_packages</member_of_group>`。** 📄 💡 构建工具靠它识别接口包，漏了会出现难懂的生成或链接错误。
4. **文件名和头文件名对不上。** 💡 `PathStatus.msg` 生成的头文件是 `path_status.hpp`（驼峰转蛇形），类型是 `PathStatus`。
5. **有界数组超过上限。** 🔍 `waypoints` 定义为 `[<=1000]`，C++ 里 `resize(1001)` 抛异常 `Exceeded upper bound`。💡 有界类型适合需要预分配内存的实时场景，但要处理越界。
6. **改了接口没重编依赖它的包。** 💡 接口的类型哈希会变，旧二进制和新二进制可能无法通信；`ros2 topic info -v` 能看到类型哈希（type hash）。📄 文档在动作教程里展示了 `RIHS01_...` 形式的哈希。
7. **想给嵌套消息字段写默认值。** 📄 不支持。

## 5. 与其他概念的关系

- [话题](topic.md)、[服务](service.md)、[动作](action.md)：分别用 `.msg`、`.srv`、`.action`
- [话题进阶](topic-advanced.md)：键控话题需要用 IDL 的 `@key` 注解
- [ament_cmake](../engineering/ament-cmake.md)：`rosidl_generate_interfaces` 也是 ament 扩展点；包结构见 [工作空间与 colcon](../engineering/workspace-colcon.md)
- [接口定义的设计](../../design/interface-design.md)（📐 设计动机）：类型映射表、有界类型在哪一层检查、builder 与 setter
- [rmw 接口与序列化的设计](../../design/rmw-serialization-design.md)（📐 设计动机）：可选字段、字典、序列化成本
- 上一级：[总览脑图](../../map/overview.md)

## 6. 自测题（先自己答，再看[答案](../../answers/interface-definition-answers.md)）

1. 你要定义一个 `.action`：目标是一组路点和最大速度，执行中反馈已完成路点数，结束时返回是否到达和总耗时。写出这个 `.action` 文件；其中"最大速度"想默认 0.5 m/s，路点用 `geometry_msgs/PoseStamped`，可以吗？
2. 一个包 `my_nav` 里既定义了 `msg/Goal.msg`，又有一个节点要发布 `Goal`。CMakeLists 里除了 `rosidl_generate_interfaces` 还要做什么？如果把消息挪到单独的 `my_nav_interfaces` 包，又要怎么写？各有什么好处？
