# 练习：组件与容器

> 适用：ROS 2 **Rolling**（rclcpp_components 33.2.0）。对应卡片：[组件与组合](../cards/concurrency/component.md)
> 代码：[fake_lidar_component.cpp](perf_lab/src/fake_lidar_component.cpp)、[cloud_filter_component.cpp](perf_lab/src/cloud_filter_component.cpp)、[lidar_container_launch.xml](perf_lab/launch/lidar_container_launch.xml)
> 用时：约 35 分钟

## 0. 编译

```bash
ln -s <你的仓库路径>/study/labs/* ~/study_ws/src/
cd ~/study_ws && source /opt/ros/rolling/setup.bash
colcon build --packages-select perf_lab && source install/setup.bash
```

两个组件编在同一个共享库 `liblidar_components.so` 里：

| 组件 | 作用 | 参数 |
|---|---|---|
| `perf_lab::FakeLidar` | 10 Hz 发布 `cloud`（`PointCloud2`，SensorDataQoS），打印前 3 帧的消息地址和 `data` 地址 | `bytes`（默认 1000000）、`rate_hz`、`publish_by_ref`、`ctor_sleep_s` |
| `perf_lab::CloudFilter` | 订阅 `cloud`，每 2 秒打印收到几帧和平均/最大延迟，打印前 3 帧地址 | `callback`：`unique` / `shared` |

## 1. 同样的组件，三种进程布局

```bash
# A：两个进程
ros2 run perf_lab fake_lidar --ros-args -p bytes:=4000000
ros2 run perf_lab cloud_filter               # 另一个终端

# B：同一容器，不开进程内通信
ros2 launch perf_lab lidar_container_launch.xml ipc:=false

# C：同一容器，开进程内通信
ros2 launch perf_lab lidar_container_launch.xml ipc:=true
```

| 布局 | 2 秒内收到几帧（应为 20） | 平均延迟 | 订阅端 `data` 地址和发布端相同吗 | 实测 |
|---|---|---|---|---|
| A | | | — | |
| B | | | | |
| C | | | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

2026-11-17 在 `ros:rolling-ros-base`（默认 `rmw_fastrtps_cpp`，4 核云主机）里实测，4 MB 点云：

| 布局 | 2 秒内收到 | 平均延迟 | 地址 |
|---|---|---|---|
| A | **0~1 帧** | 约 8~11 ms（只有那 1 帧） | — |
| B | 20 帧 | 约 3.1 ms | 不同（复制） |
| C | 20 帧 | 约 0.7 ms | **相同**（零拷贝） |

布局 A 再试不同大小：100 KB 时 20 帧都能收到（延迟约 0.8 ms），1 MB 时就只剩 0~1 帧。

注意：
- 布局 A 的大量丢帧是**跨进程传大消息 + best effort** 的问题，不是组件的问题。💡 很可能是大消息被分片后有分片丢失，整帧就丢了；属于 DDS 调优（第 7 批）。
- 布局 B 在同一进程里，但没开进程内通信，消息仍然经过中间件序列化，所以有复制、延迟更高。

</details>

## 2. 命令行加载、改名、卸载

```bash
ros2 run rclcpp_components component_container --ros-args -r __node:=MyContainer -r __ns:=/robot1
# 另一个终端
ros2 component types | grep -A2 perf_lab
ros2 component load /robot1/MyContainer perf_lab perf_lab::FakeLidar -p bytes:=1000 -e use_intra_process_comms:=true
ros2 component load /robot1/MyContainer perf_lab perf_lab::CloudFilter --node-namespace /robot1 -e use_intra_process_comms:=true
ros2 component load /robot1/MyContainer perf_lab perf_lab::CloudFilter --node-name filter2
ros2 component list
ros2 topic list | grep cloud
ros2 component unload /robot1/MyContainer 1 2 3
```

先预测：`FakeLidar` 的全名是什么？两个 `CloudFilter` 哪个能收到数据？

<details>
<summary>参考结果（填完预测再展开）</summary>

```text
Loaded component 1 into '/robot1/MyContainer' container node as '/fake_lidar'
Loaded component 2 into '/robot1/MyContainer' container node as '/robot1/cloud_filter'
Loaded component 3 into '/robot1/MyContainer' container node as '/filter2'
```

- 容器在 `/robot1` 下，但 `FakeLidar` 是 `/fake_lidar`，发布 `/cloud`：**容器的命名空间不传给组件**。
- `/robot1/cloud_filter` 订阅 `/robot1/cloud`，一直 `no cloud received`，没有任何报错。
- `/filter2` 订阅 `/cloud`，每 2 秒收到 20 帧。
- 话题列表里同时有 `/cloud` 和 `/robot1/cloud`。

launch 里也是一样：`<node exec="component_container" namespace="robot2"/>` 再用 `load_composable_node` 加载的组件不在 `/robot2` 下。但 `push_ros_namespace` 会作用到同一组里 `node_container` 下的组件（实测 `/robot1/fake_lidar`）。

</details>

## 3. 构造函数阻塞

```bash
ros2 run rclcpp_components component_container           # 单线程容器
ros2 component load /ComponentManager perf_lab perf_lab::FakeLidar -p bytes:=1000
ros2 component load /ComponentManager perf_lab perf_lab::CloudFilter
# 观察 cloud_filter 每 2 秒的输出，然后：
ros2 component load /ComponentManager perf_lab perf_lab::FakeLidar --node-name slow_lidar -p ctor_sleep_s:=6.0
```

再用 `component_container --executor-type multi-threaded` 重复一遍。

| 容器 | `slow_lidar` 构造期间，`cloud_filter` 还在输出吗 | 实测 |
|---|---|---|
| 单线程 | | |
| 多线程 | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

- 单线程：`slow_lidar` 开始睡眠后，`cloud_filter` 的下一次输出是 6 秒后，而且是 `no cloud received`。整个容器都停了：构造函数是在执行器线程里、处理 `load_node` 服务请求时执行的。
- 多线程：`cloud_filter` 照常每 2 秒收到 20 帧。
- 单线程时 `ros2 component load` 等了约 7 秒才打印 `Loaded component 3`：命令要等构造函数返回。

</details>

## 挑战（选做）

1. 把 `CloudFilter` 的参数改成 `callback:=shared`，再加载第二个 `CloudFilter`（改名）。在布局 C 里观察两个订阅者收到的 `data` 地址。把两个都改成 `unique` 再看。（提示：[进程内通信练习](intra-process-zero-copy.md)）
2. 给 `FakeLidar` 加 `-p publish_by_ref:=true`，在布局 C 里看地址和延迟。
3. 把 launch 里的 `component_container` 换成 `component_container_isolated`，用 `ps -eLf | grep component` 看线程数有什么变化。
