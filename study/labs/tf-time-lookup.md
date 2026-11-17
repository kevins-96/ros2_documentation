# 练习：lookupTransform 的时间参数

> 适用：ROS 2 **Rolling**。对应卡片：[tf2 坐标变换](../cards/coordinates/tf2.md)、[tf2 与时间](../cards/coordinates/tf2-time.md)
> 代码：[tf_lab/src/tf_time_lab.cpp](tf_lab/src/tf_time_lab.cpp)、[tf_lab/src/card_tf2_min.cpp](tf_lab/src/card_tf2_min.cpp)
> 用时：约 25 分钟

## 0. 编译

```bash
ln -s <你的仓库路径>/study/labs/* ~/study_ws/src/
cd ~/study_ws && source /opt/ros/rolling/setup.bash
colcon build --packages-select tf_lab && source install/setup.bash
```

## 1. 运行

终端 A 先启动广播者（20 Hz 发布 `odom → base_link`，静态发布 `base_link → laser`）：

```bash
ros2 run tf_lab card_tf2_min
```

终端 B 依次运行 6 种情形。每种都每 100 ms 查询一次 `odom ← laser`，跑 8 秒后打印：成功次数、失败次数、首次成功时间、结果的平均"年龄"（now 减去返回变换的时间戳）、单次查询最长阻塞时间，以及每类异常的第一条信息。

```bash
ros2 run tf_lab tf_time_lab --mode latest
ros2 run tf_lab tf_time_lab --mode now
ros2 run tf_lab tf_time_lab --mode now-timeout
ros2 run tf_lab tf_time_lab --mode past-5s
ros2 run tf_lab tf_time_lab --mode past-15s
ros2 run tf_lab tf_time_lab --mode self-broadcast
```

| `--mode` | 时间参数 | 对应的导航场景（💡） |
|---|---|---|
| `latest` | `tf2::TimePointZero` | 控制环里取机器人当前位姿 |
| `now` | `now()`，不等待 | 新手最常见的写法 |
| `now-timeout` | `now()`，最多等 50 ms | 文档推荐的修法之一 |
| `past-5s` | `now() - 5 s`，最多等 50 ms | 回看历史轨迹 |
| `past-15s` | `now() - 15 s`，最多等 50 ms | 回看超过缓存长度 |
| `self-broadcast` | 本节点自己 20 Hz 广播 `odom → self`，同一单线程执行器里用 `now()` + 50 ms 查 `odom ← self` | 一个节点既发布里程计 TF 又在定时器里查它 |

## 2. 先填预测

| `--mode` | 8 秒内大约成功几次（共约 80 次） | 失败时报什么 | 每次查询会阻塞多久 | 实测 |
|---|---|---|---|---|
| latest | | | | |
| now | | | | |
| now-timeout | | | | |
| past-5s | | | | |
| past-15s | | | | |
| self-broadcast | | | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

2026-11-17 在 `ros:rolling-ros-base`（tf2_ros 0.46.5，`rmw_fastrtps_cpp`）里实测：

| `--mode` | ok | fail | 首次成功 | 结果平均年龄 | 最长阻塞 | 主要异常 |
|---|---|---|---|---|---|---|
| latest | **78** | 1 | 0.2 s | 5.8 ms | 0.1 ms | （第 1 次）`"odom" ... does not exist` |
| now | **0** | 79 | — | — | 0.4 ms | `extrapolation into the future`（78 次） |
| now-timeout | **78** | 1 | 0.2 s | 0.0 ms | **50.3 ms** | 只有第 1 次 |
| past-5s | 29 | 50 | **5.1 s** | 5000 ms | 50.6 ms | `extrapolation into the past`（49 次） |
| past-15s | **0** | 79 | — | — | 51.4 ms | `extrapolation into the past` |
| self-broadcast | **0** | 79 | — | — | 54.0 ms | `Lookup would require extrapolation at time ...` |

解读：
- **第 1 次总是失败**：监听者刚启动，buffer 是空的，坐标系"不存在"。导航节点启动阶段要容忍这类异常。
- `latest`：拿到的变换平均比 now 旧约 6 ms，这就是广播到 buffer 的延迟。
- `now`：buffer 里最新的数据永远比 now 旧几毫秒，tf2 不外推，所以全部失败。
- `now-timeout`：等到"now 之后"的数据到达后插值出 now 时刻的变换，所以年龄是 0；代价是每次阻塞约 50 ms，在 20 Hz 控制环里这就是一整个周期。
- `past-5s`：buffer 里要攒够 5 秒历史，所以前 5 秒全部失败。
- `past-15s`：buffer 默认只存 10 秒，永远查不到。
- `self-broadcast`：等待期间单线程执行器被占住，本节点的广播定时器跑不了，等的数据永远来不了。和[执行器练习](executor-blocking.md)是同一个机制。

</details>

## 3. tf2_echo 的参数顺序

保持终端 A 运行，或者单独发布一个已知的静态变换：

```bash
ros2 run tf2_ros static_transform_publisher --x 1 --yaw 1.5708 --frame-id world --child-frame-id a
ros2 run tf2_ros tf2_echo world a
ros2 run tf2_ros tf2_echo a world
```

**预测：** 两条命令的 Translation 和 yaw 分别是多少？哪一条打印的是"a 在 world 里的位姿"？

<details>
<summary>参考结果</summary>

| 命令 | Translation | RPY (degree) |
|---|---|---|
| `tf2_echo world a` | `[1.000, 0.000, 0.000]` | `[0, 0, 90]` |
| `tf2_echo a world` | `[0.000, 1.000, 0.000]` | `[0, 0, -90]` |

`tf2_echo X Y` 打印 **Y 在 X 里的位姿**，等价于 `lookupTransform(X, Y)`。文档把参数写成 `[source_frame] [target_frame]`，按名字理解会搞反。第二条：world 原点在 a 里的坐标是 $R(-90°)\,(0-1,\ 0-0) = (0, 1)$。

</details>

## 4. 挑战（选做）

1. 把 `self-broadcast` 改好：给广播定时器单独一个回调组，并用 `MultiThreadedExecutor`。预测成功次数，再验证。（提示：[回调组](../cards/concurrency/callback-group.md)）
2. 把 `card_tf2_min` 的发布频率改成 2 Hz，重跑 `latest` 和 `now-timeout`。结果的年龄、阻塞时间、成功次数分别怎么变？
3. 运行 [card_tf2_filter_min](tf_lab/src/card_tf2_filter_min.cpp)，用 `ros2 topic pub` 发一个 `stamp` 为 0 的点、一个 `frame_id` 为空的点、一个 `frame_id` 为 `no_such_frame` 的点，观察 MessageFilter 分别怎么处理。
