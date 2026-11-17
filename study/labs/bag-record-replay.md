# 练习：录制、回放与离线分析

> 适用：ROS 2 **Rolling**。对应卡片：[rosbag 录制回放](../cards/observability/rosbag.md)、[RViz 与可视化](../cards/observability/rviz.md)
> 代码：[plan_recorder.cpp](observe_lab/src/plan_recorder.cpp)、[plan_bag_reader.cpp](observe_lab/src/plan_bag_reader.cpp)、[map_qos_override.yaml](observe_lab/config/map_qos_override.yaml)
> 用时：约 40 分钟。编译见[命令行与服务内省](cli-and-introspection.md)第 0 部分

所有命令在同一个目录里执行（比如 `mkdir -p ~/bags && cd ~/bags`）。如果你把这些命令写进脚本、在后台启动 `ros2 bag record`，脚本开头加 `set -m`，原因见第 1 部分参考结果。

## 1. 录制：地图在录制前就发完了

```bash
# 终端 A
ros2 run observe_lab fake_planner
# 终端 B（A 启动几秒后）
ros2 bag record -o b1 /map /plan /plan_markers        # 先试旧写法
ros2 bag record -o b1 --topics /map /plan /plan_markers  # 录 4 秒左右 Ctrl-C
ls b1 && ros2 bag info b1
```

| # | 问题 | 预测 | 实测 |
|---|---|---|---|
| 1 | 第一条命令（位置参数话题）能用吗？ | | |
| 2 | `b1/` 里有哪些文件？数据文件叫什么？ | | |
| 3 | `/map` 的 Count 是 0 还是 1？ | | |
| 4 | `Duration` 和你实际录的时间一样吗？ | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

2026-11-17 在 `ros:rolling-ros-base` 容器里实测：

1. 不能：`ros2: error: unrecognized arguments: /map /plan /plan_markers`。Rolling 必须写 `--topics`。
2. `0_b1_2026_11_17-16_09_55.mcap` 和 `metadata.yaml`。命名是"序号_bag 名_时间"。
3. **1**。地图是 transient local，录制器发现它后按发布者的 QoS 订阅，拿到了录制开始前发的那一条。
4. 不一样：录了约 4 秒，`Duration: 2.974691233s`，这是第一条到最后一条消息的时间差。共 13 条：`/map` 1、`/plan` 6、`/plan_markers` 6。

另外：脚本里用 `&` 后台启动 `ros2 bag record` 再 `kill -INT`，录制器**停不下来**（非交互 shell 把后台进程的 SIGINT 设为忽略），`wait` 会一直等。加 `set -m` 后正常打印 `Recording stopped`。

</details>

## 2. 回放：规划器已经停了，晚加入的订阅者还能拿到地图吗？

停掉终端 A 的 `fake_planner`。

```bash
# 终端 A
ros2 bag play b1
# 终端 B（回放开始后 1–2 秒内）
ros2 topic info -v /map | grep -E "Node name|Durab"
ros2 topic echo --once /map --field info.width
```

再试：

```bash
ros2 bag play b1 --rate 2 &  ros2 topic hz /plan          # 频率
ros2 bag play b1 --clock &   ros2 topic echo --once /clock
ros2 topic info -v /clock | grep -E "Reliab|Durab"
grep -A4 offered_qos b1/metadata.yaml
```

| # | 问题 | 预测 | 实测 |
|---|---|---|---|
| 1 | 回放时 `/map` 发布者是谁、Durability 是什么 | | |
| 2 | 晚加入的 `echo` 能收到地图吗？ | | |
| 3 | `--rate 2` 时 `/plan` 的频率 | | |
| 4 | `/clock` 的 QoS | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

1. `Node name: rosbag2_player`，`Durability: TRANSIENT_LOCAL`。播放器用了 `metadata.yaml` 里记录的 `offered_qos_profiles`。
2. 能，打印 `20`。所以在 Rolling 里，单发布者的 transient local 话题**不需要** QoS 覆盖文件。加 `--qos-profile-overrides-path map_qos_override.yaml` 结果一样。
3. `average rate: 4.002`（录的是 2 Hz）。
4. `BEST_EFFORT`、`VOLATILE`。`/clock` 的值是录制当时的时间（`sec: 1794931796`）。

</details>

## 3. 分割

```bash
ros2 run observe_lab fake_planner &           # 或在另一个终端
ros2 bag record -o b2 -d 2 --topics /plan     # 约 5 秒后 Ctrl-C
ls b2
```

预测文件名和个数。

<details>
<summary>参考结果（填完预测再展开）</summary>

```text
0_b2_2026_11_17-16_09_59.mcap
1_b2_2026_11_17-16_10_02.mcap
metadata.yaml
```

📄 这是 Lyrical 起的新命名（以前是 `b2_0.mcap`）。如果同时录 `/map`，只有第一个文件里有地图（🔍 `ros2 bag info <单个 .mcap 文件>`：第二个文件 `/map | Count: 0`）。Rolling 的 `--repeat-transient-local /map` 解决这个问题：🔍 加上后每个分割文件里 `/map` 都是 `Count: 1`。

</details>

## 4. 在自己的节点里录 bag

先看 [plan_recorder.cpp](observe_lab/src/plan_recorder.cpp)。然后做一个"错误的改法"：把

```cpp
sub_ = create_subscription<nav_msgs::msg::Path>(
```

改成 `create_subscription<rclcpp::SerializedMessage>(`（成员 `sub_` 的类型也跟着改成 `rclcpp::Subscription<rclcpp::SerializedMessage>::SharedPtr`），重新编译运行：

```bash
ros2 run observe_lab fake_planner &
ros2 run observe_lab plan_recorder --ros-args -p bag:=cpp_bag
ls cpp_bag; ros2 bag info cpp_bag
```

预测：编译能过吗？运行会怎样？`cpp_bag` 里有什么？

改回原样，重新编译后：

```bash
ros2 run observe_lab plan_recorder --ros-args -p bag:=cpp_bag2   # 4 秒后 Ctrl-C
ros2 run observe_lab plan_recorder --ros-args -p bag:=cpp_bag2   # 再来一次
ros2 bag info cpp_bag2
```

<details>
<summary>参考结果（填完预测再展开）</summary>

错误改法：编译通过，运行时立即

```text
terminate called after throwing an instance of 'std::runtime_error'
  what():  this specialization of rclcpp::get_message_type_support_handle() should never be called
[ros2run]: Aborted
```

`cpp_bag/` 里只有一个 `.mcap`，**没有** `metadata.yaml`：`ros2 bag info` 报 `Could not find metadata in bag directory cpp_bag`，`ros2 bag reindex cpp_bag` 也报 `No storage could be initialized`。Writer 析构时才写元数据，进程 abort 就没有了。

改回后：第一次正常，`Messages: 6`；第二次报 `Bag directory already exists (cpp_bag2), can't overwrite existing bag` 并 abort。`ros2 bag info` 里的话题是 **`plan`**（不带 `/`），因为 `write()` 的话题名就是代码里写的字符串。

</details>

## 5. 不回放，直接读 bag

```bash
ros2 run observe_lab plan_bag_reader cpp_bag2
ros2 run observe_lab plan_bag_reader cpp_bag2 plan
ros2 run observe_lab plan_bag_reader b1
ros2 bag play cpp_bag2 &  ros2 topic list | grep plan
```

预测前两条的区别，以及回放 `cpp_bag2` 时话题叫什么。

<details>
<summary>参考结果（填完预测再展开）</summary>

- 第一条：`topic plan [nav_msgs/msg/Path]`、`no messages on /plan`：默认按 `/plan` 过滤，一条都对不上。
- 第二条：`plan: 6 messages over 2.50 s, 20.0 points on average`，`first message: bag time - header.stamp = 0.740 ms`。
- 第三条（CLI 录的 b1）：`/plan: 6 messages over 2.50 s, 20.0 points on average`，`bag time - header.stamp = 0.338 ms`。这个差值就是从规划器打时间戳到录制器收到的延迟。
- 回放 `cpp_bag2` 时话题是 `/plan`：播放器把相对名按自己的命名空间展开了。录的时候名字"错"，回放时又"对"了，所以这类问题平时很难发现。

</details>

## 6. 录制服务

```bash
ros2 run observe_lab fake_planner --ros-args -p service_introspection:=contents &
ros2 bag record -o svc --services /replan &
ros2 service call /replan std_srvs/srv/Trigger
ros2 service call /replan std_srvs/srv/Trigger
# Ctrl-C 停止录制（或 kill -INT）
ros2 bag info svc
```

再用 `service_introspection:=disabled` 做一遍。

<details>
<summary>参考结果（填完预测再展开）</summary>

`contents`：`Messages: 0`、`Services: 1`、`Service: /replan | Type: std_srvs/srv/Trigger | Event Count: 4`（两次调用 × 服务端两个事件）。`disabled` 时没有 `_service_event` 话题，什么也录不到。

`ros2 bag record -a` 也会把服务事件录进去，同时还有 `/rosout`、`/parameter_events`、`/events/rosbag2_messages_lost`、`/events/write_split`。

</details>

## 7. Marker 检查（不需要 RViz）

```bash
ros2 run observe_lab fake_planner --ros-args -p marker_alpha:=0.0 -p frame_id:=/map
ros2 topic echo --once /plan_markers --field markers | grep -E "frame_id|  a:|ns:|  id:|lifetime" -A1
```

预测：这条 Marker 消息在 RViz 里能看到吗？如果看不到，Display 的 Status 会报什么？

<details>
<summary>参考结果（填完预测再展开）</summary>

消息本身完全正常地发出去了：`frame_id='/map'`、`ns='plan'`、`id=0` 和 `1`、`a: 0.0`、终点球 `lifetime` 2 秒。发布端不检查任何东西。

在 RViz 里会有两个问题叠加：`a = 0` 全透明，什么都看不见；`/map` 以斜杠开头，🔍 tf2 拒绝这种名字（`Invalid argument "/map" passed to lookupTransform argument target_frame - in tf2 frame_ids cannot start with a '/'`），Display 会报变换错误。💡 两个问题同时存在时，修好一个仍然"看不见"，容易误以为改错了方向。排查时先用 `echo --field` 把字段看清楚。

</details>

## 挑战（选做）

1. 改 `plan_bag_reader`：对每条 `/plan` 计算路径长度 $\sum_i \lVert p_{i+1} - p_i \rVert$ 和最大转角，输出 CSV。这是"离线回归测试"的雏形：改了规划器以后，对同一个 bag 比较前后的统计量。
2. 实现卡片自测题 2 的"出错时存最近 10 秒"：在 `fake_planner` 里缓存序列化后的 `/plan`，`/replan` 被调用时写一个带时间戳的新 bag。
3. 用 `-d 2` 分割录制 `/map` 和 `/plan`，只回放第二个文件，用一个 transient local 订阅者验证收不到地图；再加 `--repeat-transient-local /map` 重录，对比回放结果。
