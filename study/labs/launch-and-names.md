# 练习：launch 与名字解析

> 适用：ROS 2 **Rolling**（launch 3.10.3，launch_ros 0.30.3）。对应卡片：[启动参数与重映射](../cards/configuration/node-arguments.md)、[launch 启动系统](../cards/configuration/launch.md)、[日志](../cards/configuration/logging.md)
> 代码：[name_lab.cpp](config_lab/src/name_lab.cpp)、[bringup_launch.xml](config_lab/launch/bringup_launch.xml)、[bringup_launch.py](config_lab/launch/bringup_launch.py)、[card_logging_min.cpp](config_lab/src/card_logging_min.cpp)
> 用时：约 40 分钟

## 0. 编译

同[参数练习](parameter-rules.md)。

`name_lab` 创建节点 `lidar`，发布 4 个话题，代码里分别写 `scan`、`/diagnostics`、`~/status`、`sensors/scan`。它打印每个话题解析后的全名和 logger 名，然后退出（加 `--spin` 则一直运行）。

## 1. 名字解析

| # | 参数 | `scan` | `/diagnostics` | `~/status` | `sensors/scan` | logger | 实测 |
|---|---|---|---|---|---|---|---|
| 1 | （无） | | | | | | |
| 2 | `-r __ns:=/robot1 -r __node:=front_lidar -r scan:=scan_raw` | | | | | | |
| 3 | `-r __ns:=robot1` | | | | | | |
| 4 | `-r __ns:=/robot2 -r sensors/scan:=/front/points` | | | | | | |
| 5 | `-r lidar:scan:=x` | | | | | | |
| 6 | `-r other:scan:=x` | | | | | | |

命令形如 `ros2 run config_lab name_lab --ros-args <参数>`。

<details>
<summary>参考结果（填完预测再展开）</summary>

2026-11-17 在 `ros:rolling-ros-base` 里实测：

| # | `scan` | `/diagnostics` | `~/status` | `sensors/scan` | logger |
|---|---|---|---|---|---|
| 1 | `/scan` | `/diagnostics` | `/lidar/status` | `/sensors/scan` | `lidar` |
| 2 | `/robot1/scan_raw` | `/diagnostics` | `/robot1/front_lidar/status` | `/robot1/sensors/scan` | `robot1.front_lidar` |
| 3 | 节点崩溃：`Namespace not remapped to a fully qualified name (found: robot1)`，`RCLInvalidROSArgsError` | | | | |
| 4 | `/robot2/scan` | `/diagnostics` | `/robot2/lidar/status` | `/front/points` | `robot2.lidar` |
| 5 | `/x` | 不变 | 不变 | 不变 | `lidar` |
| 6 | `/scan`（规则静默无效，没有警告） | 不变 | 不变 | 不变 | `lidar` |

要点：
- #2 里 `scan:=scan_raw` 的目标是相对名，所以也进了命名空间。
- #4 目标写绝对名，才能"跳出"命名空间。
- #5、#6：节点名前缀写错不会报错。

</details>

## 2. XML launch

```bash
ros2 launch config_lab bringup_launch.xml --show-args
ros2 launch config_lab bringup_launch.xml
# 另一个终端
ros2 node list
ros2 topic list
ros2 param get /robot1/planner max_speed
ros2 param get /robot1/planner robot_id
ros2 lifecycle get /robot1/lidar_driver
```

先预测：`ros2 node list` 会列出哪些节点？`name_lab` 的 `scan` 最终是什么名字？`lidar_driver` 在不在 `/robot1` 下？

<details>
<summary>参考结果（填完预测再展开）</summary>

- `--show-args` 列出 `robot_ns`、`use_sim_time`、`enable_lidar`、`log_level`，每个都写着 `no description given`；被 include 的 `lidar_launch.xml` 的 `autostart` 也列出来了。
- 节点：`/robot1/planner`、`/robot1/lidar`、`/robot1/lidar_driver`。
- `scan` 被重映射到 `/robot1/scan_front`。
- `max_speed = 0.8`、`robot_id = bot_7`：YAML 的 `/**/planner` 匹配上了。
- `lidar_driver` 是 `active [3]`：`PushROSNamespace` 也作用到了 include 进来的文件，`autostart` 自动完成了 configure 和 activate。（lidar_launch.xml 里写的是 `namespace=""`，空字符串表示"不额外加"，所以仍在 `/robot1` 下。）

</details>

## 3. Python launch 与事件处理

```bash
ros2 launch config_lab bringup_launch.py robot_ns:=robot2 enable_lidar:=false use_sim_time:=true
# 另一个终端
ros2 node list
ros2 param get /robot2/planner use_sim_time
pkill -f card_parameter_min       # 杀掉 planner
```

<details>
<summary>参考结果（填完预测再展开）</summary>

- 节点只有 `/robot2/planner` 和 `/robot2/lidar`（`enable_lidar:=false`，没有 include）。
- `use_sim_time` 是 `True`：命令行的 launch 参数经 `parameters=[..., {'use_sim_time': use_sim_time}]` 传给了节点，并覆盖了 YAML 里的 `false`。
- 杀掉 planner 后，launch 打印 `planner exited, shutting down the whole system`，`/robot2/lidar` 也随之退出。去掉 `RegisterEventHandler` 再试，其他节点会继续运行。

</details>

## 4. 三个小实验

在一个临时 Python launch 文件里分别试：

| 实验 | 写法 | 预测 | 实测 |
|---|---|---|---|
| A | `GroupAction([Node(name='before'), PushROSNamespace('ns_x'), Node(name='after')])` | | |
| B | 节点不写 `output`，代码里同时有 `RCLCPP_INFO` 和 `printf` | | |
| C | `print(LaunchConfiguration('x') == 'true', bool(LaunchConfiguration('x')))` | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

- A：`/before` 和 `/ns_x/after`。`PushROSNamespace` 只影响它后面的动作，所以必须放在组内第一个。
- B：`RCLCPP_INFO`（走 stderr）显示在屏幕上，`printf`（stdout）**不显示**，只进日志文件。加 `output="screen"` 后两者都显示。
- C：打印 `False True`，和命令行传什么无关。替换对象要到执行时才求值。

</details>

## 5. logger 名与日志级别

`card_logging_min` 的节点叫 `battery_monitor`，有子 logger `driver`，电量每 0.5 s 掉 5%。

| # | 命令 | 预测：自己节点的 DEBUG 有几条 | 实测 |
|---|---|---|---|
| 1 | `ros2 run config_lab card_logging_min --ros-args --log-level battery_monitor:=debug` | | |
| 2 | 上面再加 `--log-level battery_monitor.driver:=warn` | | |
| 3 | `... --ros-args -r __ns:=/robot1 --log-level battery_monitor:=debug` | | |
| 4 | `... --ros-args -r __ns:=/robot1 --log-level robot1.battery_monitor:=debug` | | |
| 5 | `... --ros-args --log-level debug`，数一数谁打的最多 | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

在约 1.2 s 内：

1. `battery_monitor` 和 `battery_monitor.driver` 各 9 条。子 logger 跟随父级。
2. driver 的 DEBUG 消失，`battery_monitor` 的还在。
3. **0 条**：logger 名是 `robot1.battery_monitor`。
4. 14 条。
5. `rcl` 75 条、`rmw_fastrtps_cpp` 36 条、`rclcpp` 8 条、`rcl_logging_implementation` 6 条，自己节点只有 6 条。

另外，WARN_EXPRESSION 的 `low battery` 在每个电量档位都打了 5 次（每个 100 ms 周期一次），而 1000 ms 的 INFO_THROTTLE 在 5.5 s 里只打了 6 次。

</details>

## 挑战（选做）

1. 把 [bringup_launch.xml](config_lab/launch/bringup_launch.xml) 改成两台机器人：用两个 `<group>` 分别 push `robot1`、`robot2`，并让两台的 planner 用不同的 `max_speed`（提示：[参数答案](../answers/parameter-answers.md)题 2）。
2. 给 bringup 的每个 `<arg>` 加上 `description`，再看 `--show-args`。
3. 给 Python 版的 `name_lab` 节点加 `respawn=True, respawn_delay=2.0`，杀掉它，观察 launch 的输出。
