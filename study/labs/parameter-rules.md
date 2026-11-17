# 练习：参数的规则与陷阱

> 适用：ROS 2 **Rolling**。对应卡片：[参数](../cards/configuration/parameter.md)
> 代码：[config_lab/src/card_parameter_min.cpp](config_lab/src/card_parameter_min.cpp)、[config_lab/src/param_watcher.cpp](config_lab/src/param_watcher.cpp)、[planner.yaml](config_lab/config/planner.yaml)、[planner_bad.yaml](config_lab/config/planner_bad.yaml)
> 用时：约 25 分钟

## 0. 编译

```bash
ln -s <你的仓库路径>/study/labs/* ~/study_ws/src/
cd ~/study_ws && source /opt/ros/rolling/setup.bash
colcon build --packages-select config_lab && source install/setup.bash
```

节点 `planner` 声明的参数：

| 参数 | 类型 | 默认值 | 限制 |
|---|---|---|---|
| `max_speed` | double | 0.5 | 范围 0~2（描述符） |
| `max_accel` | double | 1.0 | pre-set 回调：改 `max_speed` 时自动设为 `2 × max_speed` |
| `robot_id` | string | `bot_0` | 只读 |
| `controller.kp` | double | 1.0 | set 回调拒绝负数 |
| `controller.ki` | double | 0.0 | — |
| `frame_ids` | string[] | `[map, odom]` | — |

## 1. 运行时修改

终端 A：`ros2 run config_lab card_parameter_min`
终端 B：`ros2 run config_lab param_watcher`（监听 `/planner` 的 `max_speed`）
终端 C 依次执行下面的命令，先填预测。

| # | 命令 | 成功？报什么 | 实测 |
|---|---|---|---|
| 1 | `ros2 param set /planner max_speed 1.5` | | |
| 2 | `ros2 param get /planner max_accel` | | |
| 3 | `ros2 param set /planner max_speed 1` | | |
| 4 | `ros2 param set /planner max_speed 3.0` | | |
| 5 | `ros2 param set /planner robot_id bot_9` | | |
| 6 | `ros2 param set /planner controller.kp -1.0` | | |
| 7 | `ros2 param set /planner not_declared 1.0` | | |
| 8 | `ros2 param set /planner frame_ids "[off]"` | | |
| 9 | `ros2 param set /planner frame_ids "[!!str off]"` | | |
| 10 | `ros2 param set /planner frame_ids "[map, off, 1]"` | | |
| 11 | `ros2 param delete /planner max_speed` | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

2026-11-17 在 `ros:rolling-ros-base`（rclcpp 33.2.0）里实测：

1. 成功。终端 A 打印 post-set 回调的 `applied ...`，终端 B 的 `ParameterEventHandler` 收到 `max_speed = 1.5`。
2. `3.0`。pre-set 回调把 `max_accel` 加进了同一批修改。
3. 失败：`Wrong parameter type, parameter {max_speed} is of type {double}, setting it to {integer} is not allowed`。
4. 失败：`doesn't comply with floating point range`。
5. 失败：`parameter 'robot_id' cannot be set because it is read-only`。
6. 失败，原因是 set 回调里写的 `controller.kp must be >= 0`。
7. 失败：`parameter 'not_declared' cannot be set because it was not declared`。
8. 失败：`[off]` 被 YAML 解析成 `bool_array`，类型不对。
9. 成功（`"[\"off\"]"` 也成功）。
10. 失败：异构列表被当成一个字符串，类型不对。
11. 失败：`cannot undeclare a statically typed parameter`。

</details>

## 2. 启动时给值

| # | 命令 | 结果预测 | 实测 |
|---|---|---|---|
| 12 | `ros2 run config_lab card_parameter_min --ros-args --params-file <share>/config/planner.yaml` | | |
| 13 | `... --ros-args -p max_speed:=1` | | |
| 14 | `... --ros-args -p max_speed:=5.0` | | |
| 15 | `... --ros-args -p controller.kp:=-1.0` | | |
| 16 | `... --ros-args -p max_speed:=1.2`，然后 `ros2 param get /planner max_accel` | | |
| 17 | `... --ros-args --params-file <share>/config/planner_bad.yaml` | | |
| 18 | `... --ros-args -r __ns:=/robot1 --params-file` 一个键写成 `planner:` 的文件 | | |

`<share>` 用 `$(ros2 pkg prefix config_lab)/share/config_lab` 代替。

<details>
<summary>参考结果（填完预测再展开）</summary>

12. `max_speed = 0.8`、`robot_id = bot_7`、`controller.kp = 2.0`。`/**/planner` 匹配上了。
13. **节点崩溃**：`parameter 'max_speed' has invalid type: Wrong parameter type ...`（`InvalidParameterTypeException`），`[ros2run]: Aborted`。启动值的类型错误不是"设置失败"，而是构造函数抛异常。
14. **节点崩溃**：`doesn't comply with floating point range`。描述符的范围对启动值有效。
15. **被接受**，kp = -1.0。set 回调在 `declare_parameter` 之后才注册，拦不住启动值。
16. `max_accel` 仍是 1.0。pre-set 回调同样不处理启动值，联动没有发生。
17. `planner_bad.yaml` 里有两段：`planer:`（拼错）和 `planner: max_speed: 1`（整数）。拼错的那段被**静默忽略**；整数那段让节点**崩溃**。
18. 不匹配，节点用默认值启动，不报错。改成 `/robot1/planner:` 或 `/**/planner:` 才匹配。

</details>

## 3. 想一想

1. 15 和 16 说明回调不处理启动值。如果你的节点必须保证 `kp >= 0`，构造函数里该怎么写？
2. 17 里拼错的键为什么不报错？📄 一个 YAML 文件可以同时给很多节点设参数，每个节点只取匹配自己的段，所以"有一段没人用"并不是错误。你会怎样在启动后快速发现这种问题？

## 挑战（选做）

1. 给 `max_accel` 加一个只读描述符，再执行 #1。pre-set 回调添加的修改会被拒绝吗？整批修改会怎样？
2. 用 `ros2 param dump /planner` 导出参数，改掉一个值，用 `ros2 param load` 加载回去，观察终端 A 的回调输出。
