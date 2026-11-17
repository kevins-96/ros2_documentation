---
tags: [ros2, rclcpp, 日志, logging]
distro: rolling
status: batch-3
aliases: [日志, logging, RCLCPP_INFO, log level, 日志级别, rosout, rqt_console, THROTTLE]
---

# 日志 Logging

> 适用：ROS 2 **Rolling**（2026-11-17 在 `ros:rolling-ros-base` 上编译实测，rclcpp 33.2.0）
> 来源标记：📄 仓库文档原文 · 🔍 源码或实测核实 · 💡 经验补充（可信，但请自己验证）
> 源文档：[About-Logging.rst](../../../source/ROS-Framework/nodes/About-Logging/About-Logging.rst)、[Logging-and-logger-configuration.rst](../../../source/Developer-Tools/Introspection-and-analysis/Logging-and-logger-configuration.rst)、[Learning-About-Logging.rst](../../../source/ROS-Framework/nodes/Working-with-nodes/Learning-About-Logging/Learning-About-Logging.rst)、[Using-Rqt-Console.rst](../../../source/ROS-Framework/nodes/Working-with-nodes/Using-Rqt-Console/Using-Rqt-Console.rst)、[Node-arguments.rst](../../../source/Developer-Tools/Launch/Node-arguments.rst)（日志部分）

## 1. 一句话定义

ROS 2 日志按严重级别（severity level）`DEBUG < INFO < WARN < ERROR < FATAL` 过滤消息，默认同时输出到三个地方：**控制台（stderr）**、**磁盘日志文件**、**`/rosout` 话题**。📄

## 2. 为什么需要

💡 机器人在现场出问题时，日志常常是唯一的线索。用好级别和节流，平时日志干净，出问题时能临时打开 DEBUG 看细节，而不用重新编译。

### 关键规则 📄

- 每个节点自带一个 logger，名字包含节点名和命名空间（🔍 实测 `/robot1/front_lidar` 的 logger 名是 `robot1.front_lidar`，用 `.` 分隔）。节点被重映射改名后，logger 名跟着变。
- logger 名有层级：`abc.def` 没设级别时继承 `abc` 的级别，再没有就用默认级别。
- 默认级别是 `INFO`，只有 DEBUG 被隐藏。
- 日志目录：`ROS_LOG_DIR`（非空时）> `$ROS_HOME/log` > `~/.ros/log`。
- 环境变量对整个**进程**生效：`RCUTILS_CONSOLE_OUTPUT_FORMAT`（格式）、`RCUTILS_COLORIZED_OUTPUT`（颜色）、`RCUTILS_LOGGING_USE_STDOUT`（改用 stdout）、`RCUTILS_LOGGING_BUFFERED_STREAM`、`RCL_LOGGING_SPDLOG_FLUSH_PERIOD_SECONDS`（日志文件刷盘周期，默认 5 s，ERROR 立即刷）。

### rclcpp 宏 📄

| 后缀 | 含义 |
|---|---|
| （无） | 每次都打 |
| `_ONCE` | 只在第一次执行到时打 |
| `_EXPRESSION` | 表达式为真时打 |
| `_FUNCTION` | 函数返回真时打 |
| `_SKIPFIRST` | 第一次不打，之后都打 |
| `_THROTTLE` | 最多每 N 毫秒打一次（要传 clock） |
| `_SKIPFIRST_THROTTLE` | 节流，且跳过第一次 |
| `_STREAM*` | 用 `<<` 流式拼接，可与上面组合 |

📄 消息末尾不用加 `\n`。

## 3. 最小 C++ 代码（rclcpp）

[card_logging_min.cpp](../../labs/config_lab/src/card_logging_min.cpp)：

```cpp
// 开启外部改级别的服务（默认关闭）📄
BatteryMonitor() : Node("battery_monitor", rclcpp::NodeOptions().enable_logger_service(true))
{
  driver_logger_ = get_logger().get_child("driver");   // battery_monitor.driver
}

void tick()
{
  RCLCPP_DEBUG(get_logger(), "tick %d level %d%%", ticks_, level_);              // 默认看不到
  RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 1000, "level %d%%", level_);  // 最多 1 次/秒
  RCLCPP_WARN_EXPRESSION(get_logger(), level_ <= 30, "low battery %d%%", level_);
  RCLCPP_ERROR_ONCE(get_logger(), "critical battery, stop now");
  RCLCPP_DEBUG_STREAM(driver_logger_, "raw voltage " << v);
}
```

运行时调整：

```bash
# 启动时 📄
ros2 run config_lab card_logging_min --ros-args --log-level debug                    # 默认级别
ros2 run config_lab card_logging_min --ros-args --log-level battery_monitor:=debug   # 单个 logger
export RCUTILS_CONSOLE_OUTPUT_FORMAT="[{severity}] [{name}] {short_file_name}:{line_number} {message}"

# 运行中（需要 enable_logger_service）📄
ros2 service call /battery_monitor/set_logger_levels rcl_interfaces/srv/SetLoggerLevels \
  "{levels: [{name: battery_monitor, level: 10}]}"     # 10=DEBUG 20=INFO 30=WARN 40=ERROR 50=FATAL

ros2 run rqt_console rqt_console      # 图形界面：收集、过滤、保存 /rosout 上的日志 📄
```

📄 关闭某个输出目标：`--disable-stdout-logs`、`--disable-rosout-logs`（省网络带宽，但外部看不到日志）、`--disable-external-lib-logs`（不写文件）。

## 4. 常见坑

🔍 以下结果都在 [card_logging_min](../../labs/config_lab/src/card_logging_min.cpp) 上实测，电量每 0.5 s 掉 5%，定时器 10 Hz。

1. **`_EXPRESSION` 不节流，会刷屏。** 电量停在 30% 的 0.5 s 里，`low battery 30%` 打了 **5 次**（每个定时器周期一次），25%、20%、15% 也各 5 次。同样 5.5 s 里，`INFO_THROTTLE`（1000 ms）只打了 6 次。💡 状态类告警用 `_THROTTLE`，或者只在状态变化时打。
2. **命名空间下 `--log-level 节点名:=debug` 不生效。** 节点在 `/robot1` 下时，`--log-level battery_monitor:=debug` 得到 **0** 条 DEBUG，`--log-level robot1.battery_monitor:=debug` 得到 14 条。logger 名是 `命名空间.节点名`，用 `.` 而不是 `/` 分隔。
3. **子 logger 默认跟随父级。** 只设 `battery_monitor:=debug` 时，`battery_monitor.driver` 的 DEBUG 也会输出（1.2 s 内两者各 9 条）。再加 `--log-level battery_monitor.driver:=warn` 后，driver 的 DEBUG 消失。📄 层级继承规则就是这样。
4. **全局 `--log-level debug` 会淹没你自己的日志。** 1.2 s 内 `rcl` 打了 75 条、`rmw_fastrtps_cpp` 36 条，你自己节点的只有 6 条。💡 调试时只开自己 logger 的 DEBUG。
5. **外部改级别的服务默认不存在。** 只有 `enable_logger_service(true)` 的节点才有 `~/get_logger_levels` 和 `~/set_logger_levels`。实测通过服务改成 DEBUG 后，0.6 s 内出现 16 条 DEBUG。📄 这两个服务不是线程安全的，同一时间只能有一个调用者。
6. **日志文件越积越多。** 本次实测的容器里，`~/.ros/log` 下已经有 62 个文件。每次 `ros2 run` 都会新建一个。💡 在机器人上要定期清理，或者把 `ROS_LOG_DIR` 指到有容量规划的分区。
7. **`THROTTLE` 用的是节点时钟。** 📄 宏要传 `*get_clock()`。💡 `use_sim_time=true` 且回放暂停时，仿真时间不走，节流期一直不结束，日志看起来就像"卡住"了。
8. **DEBUG 语句里的计算也有开销。** 📄 文档的 demo 演示了这一点：DEBUG 没启用时，`_FUNCTION`、`_EXPRESSION` 里的函数和表达式都不会求值。💡 但普通 `RCLCPP_DEBUG` 的参数如果是函数调用，C++ 不保证不求值。要避免高频路径里做昂贵的格式化。

## 5. 与其他概念的关系

- [启动参数与重映射](node-arguments.md)：`--log-level` 等命令行参数，logger 名跟着节点名变
- [launch 启动系统](launch.md)：`ros_args="--log-level ..."`；`output="screen"` 与 stdout 的关系
- [话题](../communication/topic.md)：`/rosout` 就是一个普通话题
- 调试与观测（第 6 批）：rosbag、tracing
- 上一级：[总览脑图](../../map/overview.md)

## 6. 自测题（先自己答，再看[答案](../../answers/logging-answers.md)）

1. 控制器节点在 `/robot3` 命名空间下，名字是 `controller`，它内部还有一个子 logger `controller.mpc`。你只想看 MPC 求解器的 DEBUG 输出，不想看控制器其他部分和 ROS 内部的 DEBUG。写出 `--ros-args` 后面的参数。
2. 路径跟踪控制器 50 Hz 运行，偏离路径超过 0.3 m 时打一条 WARN。现场反馈"日志刷屏，看不到别的信息"。给出两种改法，并说明各自的取舍。
