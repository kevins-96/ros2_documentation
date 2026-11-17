# 练习：命令行与服务内省

> 适用：ROS 2 **Rolling**。对应卡片：[命令行工具](../cards/observability/cli-tools.md)、[服务与动作内省](../cards/observability/service-introspection.md)
> 代码：[fake_planner.cpp](observe_lab/src/fake_planner.cpp)、[graph_probe.cpp](observe_lab/src/graph_probe.cpp)、[goals.yaml](observe_lab/config/goals.yaml)
> 用时：约 30 分钟

## 0. 编译

```bash
ln -s <你的仓库路径>/study/labs/observe_lab ~/study_ws/src/   # 已按 README 链接过就跳过
cd ~/study_ws && colcon build --packages-select observe_lab
source install/setup.bash
```

`fake_planner` 是本批所有练习的"被观察对象"：

| 接口 | 说明 |
|---|---|
| `/map`（`OccupancyGrid`） | 20×20、0.5 m 栅格，中间一堵墙；**transient local + reliable、depth 1，启动时只发一次** |
| `/plan`（`Path`） | 按 `rate_hz`（默认 2 Hz）发布锯齿路径，终点跟随 `/goal_pose` |
| `/plan_markers`（`MarkerArray`） | 路径线 + 终点球 |
| `/goal_pose`（订阅 `PoseStamped`） | 收到后打印 `new goal (x, y)` |
| `/replan`（`std_srvs/Trigger`） | 参数 `service_introspection` = `disabled` / `metadata` / `contents` |
| 其他参数 | `work_ms`（回调里忙等多久）、`crash_on_replan`、`marker_alpha`、`frame_id` |

## 1. 看清 ROS 图

终端 A：`ros2 run observe_lab fake_planner`。终端 B 依次执行：

```bash
ros2 node info /fake_planner
ros2 topic list --include-hidden-topics
ros2 topic hz /plan          # Ctrl-C 结束
ros2 topic bw /plan
ros2 topic info -v /map
```

| # | 问题 | 预测 | 实测 |
|---|---|---|---|
| 1 | `node info` 里 Service Servers 有几个？除了 `/replan` 还有什么？ | | |
| 2 | 隐藏话题里有没有 `/replan/_service_event`？ | | |
| 3 | `/plan` 的频率、每条大小 | | |
| 4 | `/map` 发布者的 Reliability、Durability、Depth | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

2026-11-17 在 `ros:rolling-ros-base` 容器里实测：

1. 8 个：`/replan` 加 7 个参数服务（`describe_parameters`、`get_parameter_types`、`get_parameters`、`get_type_description`、`list_parameters`、`set_parameters`、`set_parameters_atomically`），每个节点默认都有。
2. 没有（内省默认关）。但多了一个 `/plan_markers/_buf_cpu`：Rolling 的 `rmw_fastrtps_cpp` 缓冲后端机制带来的，`ros2 topic info -v` 显示节点名 `_NODE_NAME_UNKNOWN_`、类型哈希 `INVALID`。
3. `average rate: 2.000`；`3.11 KB/s from 2 messages, Message size mean: 1.47 KB`。20 个 `PoseStamped`，每个带完整 header，所以比 20 个点的坐标本身大得多。
4. `RELIABLE`、`TRANSIENT_LOCAL`、`KEEP_LAST (1)`。

</details>

## 2. 晚加入的订阅者

`fake_planner` 已经运行了一会儿（地图早就发完了）：

```bash
ros2 topic echo --once /map --field info
timeout 5 ros2 topic echo --once /map --qos-durability volatile --field info; echo "rc=$?"
```

预测两条命令各自能不能收到地图。

<details>
<summary>参考结果（填完预测再展开）</summary>

- 第一条立即打印 `resolution: 0.5`、`width: 20`、`height: 20` 等：`ros2 topic echo` 按发布者的 QoS 自动选了 transient local，晚加入也能拿到最后一条。
- 第二条一直等到 `timeout`，`rc=124`：强制 volatile 后，只能收到"之后"发布的消息，而地图不会再发。

你自己的节点如果用默认 QoS 订阅地图，就是第二种情况。这就是"CLI 能看到、节点收不到"的典型原因。

</details>

## 3. 用 YAML 发多个目标

[goals.yaml](observe_lab/config/goals.yaml) 里有三个 `PoseStamped`，`x` 依次是 1、2、3。终端 A 的 `fake_planner` 保持运行：

```bash
ros2 topic pub --times 3 --yaml-file <你的仓库路径>/study/labs/observe_lab/config/goals.yaml \
  /goal_pose geometry_msgs/msg/PoseStamped
ros2 topic pub --times 3 --max-wait-time-secs 3 --yaml-file <同上> \
  /nobody geometry_msgs/msg/PoseStamped; echo "rc=$?"
```

| # | 问题 | 预测 | 实测 |
|---|---|---|---|
| 1 | 终端 A 里 `new goal` 的顺序和时间间隔 | | |
| 2 | 第二条命令（没人订阅 `/nobody`）多久结束、退出码 | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

1. `new goal (1.0, 0.0)`、`(2.0, 0.0)`、`(3.0, 0.0)`，时间戳相差约 1.00 s（默认 `-r 1`）。📄 文档的示例输出是 1、3、2，那是笔误，实际按文件顺序发。
2. 约 4 秒后打印 `Timed out waiting for subscribers: Expected 1 subscriber but only got 0 subscribers`，`rc=1`。不加 `--max-wait-time-secs` 会一直等：`--times` 让 `-w` 默认为 1。

</details>

## 4. ros2 doctor 与 daemon

```bash
ros2 doctor
ros2 doctor --report | grep -A12 "TOPIC LIST"
ros2 daemon stop; ros2 daemon status
time ros2 topic list > /dev/null
time ros2 topic list > /dev/null
time ros2 topic list --no-daemon > /dev/null
```

预测：doctor 会报哪些警告？最后一行是 passed 还是 failed？三次 `topic list` 哪次最快？

<details>
<summary>参考结果（填完预测再展开）</summary>

```text
UserWarning: Cannot find the latest versions of packages: observe_lab. ...
UserWarning: Subscriber without publisher detected on /goal_pose.
UserWarning: Publisher without subscriber detected on /map.
UserWarning: Publisher without subscriber detected on /plan.
UserWarning: Publisher without subscriber detected on /plan_markers.
UserWarning: Publisher without subscriber detected on /plan_markers/_buf_cpu.

All 5 checks passed
```

只有警告时仍然 `passed`。`observe_lab` 那条是因为本地包不在 ROS 官方索引里，可以忽略。

`topic list` 用时：第一次（daemon 已停，顺带启动它）约 1.0 s，第二次（daemon 在运行）约 0.37 s，`--no-daemon` 约 1.0 s。

</details>

## 5. 服务内省的三种状态

对每个状态做一遍（终端 A 重启 `fake_planner`）：

```bash
# 终端 A
ros2 run observe_lab fake_planner --ros-args -p service_introspection:=metadata
# 终端 B
ros2 service echo /replan
# 终端 C
ros2 service call /replan std_srvs/srv/Trigger
```

| 状态 | 预测：终端 B 看到什么？每次调用几条事件？ | 实测 |
|---|---|---|
| `disabled` | | |
| `metadata` | | |
| `contents` | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

- `disabled`：`No publishers on topic '/replan/_service_event'; is service introspection on the client or server enabled?`
- `metadata`：每次调用两条，`event_type: REQUEST_RECEIVED` 和 `RESPONSE_SENT`，带 `stamp`、`client_gid`（16 个数字）、`sequence_number: 1`；`request: []`、`response: []`。
- `contents`：同样两条，`request: - {}`（Trigger 请求本来就是空结构），响应事件里有 `success` 和 `message`。

只有两条，是因为 `ros2 service call` 这个客户端没有开内省：`REQUEST_SENT` 和 `RESPONSE_RECEIVED` 由客户端发布。

</details>

## 6. 用代码查图

```bash
ros2 run observe_lab graph_probe
ros2 run observe_lab graph_probe --ros-args -p topic:=/goal_pose
ros2 run observe_lab graph_probe --ros-args -p topic:=map
```

预测：`graph_probe` 列出的话题和 `ros2 topic list` 一样吗？第三条用相对名 `map` 能查到吗？

<details>
<summary>参考结果（填完预测再展开）</summary>

- `get_topic_names_and_types()` **包含**隐藏话题 `/plan_markers/_buf_cpu`；`ros2 topic list` 默认把它过滤掉了。
- `/goal_pose: 0 publishers, 1 subscribers`（`graph_probe` 自己不订阅，不算）。
- `map` 按节点命名空间展开成 `/map`，结果和 `/map` 一样：`1 publishers`，`reliability=reliable durability=transient_local depth=1`。

</details>

## 挑战（选做）

1. 改 `fake_planner`：把 `service_introspection` 做成运行时可改的参数（参数回调里重新调用 `configure_introspection`），然后用 `ros2 param set` 在不重启的情况下切换三种状态。
2. 写一个带内省的客户端（`client->configure_introspection(...)`），调用 `/replan`，验证 `ros2 service echo` 能看到完整的 4 种事件，并用 `sequence_number` 和 `client_gid` 把请求和响应配对。
3. 改 `graph_probe`：等待指定话题出现发布者（每 100 ms 检查一次，超时 5 s 退出码 1），并在发布者 QoS 是 best effort 时打印警告。这就是很多 Nav2 节点启动时"等待上游"的写法。
