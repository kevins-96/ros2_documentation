---
tags: [ros2, 命令行, 重映射, 命名空间, 名字解析]
distro: rolling
status: batch-3
aliases: [启动参数, 重映射, remap, ros-args, 命名空间, namespace, 名字解析, name resolution]
---

# 启动参数与重映射

> 适用：ROS 2 **Rolling**（2026-11-17 在 `ros:rolling-ros-base` 上实测，rclcpp 33.2.0）
> 来源标记：📄 仓库文档原文 · 🔍 源码或实测核实 · 💡 经验补充（可信，但请自己验证）
> 源文档：[Node-arguments.rst](../../../source/Developer-Tools/Launch/Node-arguments.rst)、[Configuring-ROS2-Environment.rst](../../../source/Get-Started/Configuring-ROS2-Environment.rst)（只收 source 部分，Domain ID 与发现放第 7 批）
> 📐 设计动机：[名字与重映射的设计](../../design/remapping-design.md)：规则的匹配和应用顺序、为什么用 `--ros-args`，以及设计里的通配符在 Rolling 还没实现

## 1. 一句话定义

`--ros-args` 之后的命令行参数用来在**不改代码**的情况下改节点名、命名空间、话题名、参数和日志级别；其中重映射（remapping）是在节点启动时把代码里写的名字替换成另一个名字。📄

## 2. 为什么需要

- 同一个激光驱动要跑两份（前、后各一个），节点名和话题名必须不同。📄 文档的例子：两个 turtlesim 靠不同命名空间共存。
- 第三方节点订阅 `/scan`，你的驱动发 `/front/scan`，重映射一下就能连上，不用改任何一方的代码。
- 💡 launch 文件里的 `name`、`namespace`、`remap`、`param` 最终都会翻译成这些命令行参数。launch 日志里的 `process has died ... cmd '...'` 能看到完整命令，排查时很有用。

### 名字解析规则

一个名字最终变成什么，取决于写法（🔍 [name_lab](../../labs/config_lab/src/name_lab.cpp) 实测，节点名 `lidar`）：

| 代码里的写法 | 种类 | 默认 | `-r __ns:=/robot1 -r __node:=front_lidar` 后 |
|---|---|---|---|
| `scan` | 相对名（relative） | `/scan` | `/robot1/scan` |
| `sensors/scan` | 相对名 | `/sensors/scan` | `/robot1/sensors/scan` |
| `/diagnostics` | 绝对名（absolute） | `/diagnostics` | `/diagnostics`（不受命名空间影响） |
| `~/status` | 私有名（private） | `/lidar/status` | `/robot1/front_lidar/status` |
| 节点 logger | — | `lidar` | `robot1.front_lidar` |

💡 写可复用节点时，话题用**相对名**（便于整体放进命名空间），节点自己的诊断信息用 `~/` 私有名，只有全局唯一的东西（如 `/tf`、`/clock`）才用绝对名。

## 3. 用法速查 📄

```bash
# 所有 ROS 参数都放在 --ros-args 之后
ros2 run pkg exe --ros-args -r __node:=front_lidar -r __ns:=/robot1 \
  -r scan:=scan_raw -p max_speed:=1.0 --params-file planner.yaml \
  --log-level debug --log-level robot1.front_lidar:=debug

# 一个进程里有多个节点（组件）时，加节点名前缀只对该节点生效
ros2 run composition manual_composition --ros-args \
  -r talker:__node:=my_talker -r my_talker:chatter:=my_topic
```

| 参数 | 作用 |
|---|---|
| `-r __node:=新名` | 改节点名 |
| `-r __ns:=/命名空间` | 改命名空间，必须以 `/` 开头 |
| `-r 旧名:=新名` | 话题、服务重映射 |
| `-r 节点名:旧名:=新名` | 只对某个节点生效的重映射 |
| `-p 名:=值` | 设参数（值按 YAML 解析） |
| `--params-file x.yaml` | 从 YAML 加载参数 |
| `--log-level 级别` / `--log-level logger名:=级别` | 默认日志级别 / 单个 logger 的级别 |

- 📄 先改节点名和命名空间，再做话题重映射。所以上面的第二条要用新名字 `my_talker:` 作前缀。
- 📄 这些重映射是**静态的**，在节点的整个生命周期内有效；节点启动后不能动态重映射。
- 📄 其他与日志相关的参数：`--log-file-name`、`--log-config-file`、`--disable-stdout-logs`、`--disable-rosout-logs`、`--disable-external-lib-logs`（见[日志](logging.md)）。
- 📄 不 `source` 安装目录里的 setup 文件，`ros2` 命令和包都找不到；每个新终端都要 source。

## 4. 常见坑

1. **`__ns` 没写开头的 `/`。** 🔍 `-r __ns:=robot1` 报 `Namespace not remapped to a fully qualified name (found: robot1)`，然后 `RCLInvalidROSArgsError`，**节点直接崩溃**。
2. **以为重映射只改绝对名。** 🔍 `-r scan:=scan_raw` 在 `/robot1` 下得到 `/robot1/scan_raw`：重映射规则和目标名都按相对名解析。要映射到别的命名空间，目标写绝对名，如 `-r sensors/scan:=/front/points`。
3. **代码里写了绝对名，命名空间就失效了。** 🔍 `/diagnostics` 加了 `__ns` 后仍是 `/diagnostics`。💡 多机器人场景下，两台机器人的 `/cmd_vel` 会互相串，排查时先查代码里有没有以 `/` 开头的话题名。
4. **节点名前缀写错，静默无效。** 🔍 `-r other:scan:=x` 对节点 `lidar` 不生效，话题仍是 `/scan`，没有任何警告。
5. **ROS 参数没放在 `--ros-args` 后面。** 📄 程序会把它当成普通参数。💡 想同时传自己的参数，要用 `rclcpp::remove_ros_arguments(argc, argv)` 把 ROS 参数去掉后再解析，练习里的 `name_lab --spin` 就是这么做的。
6. **YAML 键和节点全名对不上。** 🔍 节点运行在 `/robot1/planner` 时，YAML 键写 `planner:` 不匹配（见[参数](parameter.md)坑 3）。

## 5. 与其他概念的关系

- [节点与 ROS 图](../communication/node.md)：节点名、命名空间是 ROS 图里的身份
- [参数](parameter.md)：`-p`、`--params-file`
- [launch 启动系统](launch.md)：`name`、`namespace`、`remap` 最终都翻译成这些参数
- [日志](logging.md)：`--log-level`，logger 名跟着节点名变化
- [tf2 坐标变换](../coordinates/tf2.md)：`/tf` 是绝对名，多机器人一般用 frame 前缀而不是重映射 `/tf`
- 练习：[launch 与名字解析](../../labs/launch-and-names.md)
- [名字与重映射的设计](../../design/remapping-design.md)（📐 设计动机）：`rostopic://` / `rosservice://` 只改一种名字、动作名的重映射
- 上一级：[总览脑图](../../map/overview.md)

## 6. 自测题（先自己答，再看[答案](../../answers/node-arguments-answers.md)）

1. 一个节点 `odom_node`，代码里发布 `odom`、`/tf`、`~/debug`，订阅 `wheel/ticks`。用 `--ros-args -r __ns:=/robot2 -r wheel/ticks:=/hw/encoders` 运行后，这 4 个名字分别变成什么？
2. 你要在同一台电脑上运行前后两个激光驱动（同一个可执行文件 `lidar_node`，代码里发布相对名 `scan`，节点名 `lidar`），让它们分别发布 `/front/scan` 和 `/rear/scan`。至少给出两种命令行写法，并说明各自的节点全名。
