# 练习：生命周期状态切换

> 适用：ROS 2 **Rolling**。对应卡片：[生命周期节点](../cards/configuration/lifecycle-node.md)
> 代码：[config_lab/src/card_lifecycle_min.cpp](config_lab/src/card_lifecycle_min.cpp)、[lidar_launch.xml](config_lab/launch/lidar_launch.xml)
> 用时：约 25 分钟

## 0. 编译

同[参数练习](parameter-rules.md)。

节点 `lidar_driver` 在 configure 时创建 `LifecyclePublisher`（`/scan_status`）和一个定时器，定时器每次触发都发布 `scan N`（N 一直累加）。几个参数用来制造故障：

| 参数 | 作用 |
|---|---|
| `fail_configure:=true` | `on_configure` 返回 FAILURE |
| `error_configure:=true` | `on_configure` 返回 ERROR |
| `skip_base_activate:=true` | `on_activate` 不调用 `LifecycleNode::on_activate` |
| `rate_hz` | 定时器频率，默认 2.0 |

## 1. 正常流程

终端 A：`ros2 run config_lab card_lifecycle_min`
终端 B：`ros2 topic echo /scan_status`
终端 C：

```bash
ros2 lifecycle get /lidar_driver
ros2 lifecycle set /lidar_driver activate      # 跳过 configure 会怎样？
ros2 lifecycle set /lidar_driver configure
# 等 5 秒
ros2 lifecycle set /lidar_driver activate
ros2 lifecycle set /lidar_driver deactivate
ros2 lifecycle set /lidar_driver cleanup
ros2 lifecycle set /lidar_driver shutdown
```

| 问题 | 预测 | 实测 |
|---|---|---|
| unconfigured 时直接 activate 的结果 | | |
| configure 后、activate 前，终端 B 收到消息吗？ | | |
| activate 后终端 B 收到的第一条是 `scan 1` 吗？ | | |
| inactive 时终端 A 打印什么？ | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

2026-11-17 在 `ros:rolling-ros-base`（rclcpp_lifecycle 33.2.0）里实测：

- `ros2 lifecycle get` 显示 `unconfigured [1]`。
- 直接 activate：`Unknown transition requested, available ones are: - configure [1]`，状态不变。
- configure 后终端 B **收不到**消息：发布者还没激活。
- activate 后收到的第一条是 **`scan 12`**，不是 `scan 1`：定时器从 configure 起就在跑，未激活期间 `publish()` 被调用了，只是消息被丢弃。
- 未激活时终端 A 只打印**一条** warning：`Trying to publish message on the topic '/scan_status', but the publisher is not activated`，之后不再重复，直到下一次进入未激活状态。
- 生命周期节点比普通节点多出这些接口：`~/change_state`、`~/get_state`、`~/get_available_states`、`~/get_available_transitions`、`~/get_transition_graph` 服务和 `~/transition_event` 话题。

</details>

## 2. 制造故障

```bash
ros2 run config_lab card_lifecycle_min --ros-args -p fail_configure:=true
ros2 run config_lab card_lifecycle_min --ros-args -p error_configure:=true
ros2 run config_lab card_lifecycle_min --ros-args -p skip_base_activate:=true
```

对每种情况执行 configure（第三种再 activate），然后 `ros2 lifecycle get`。

| 情况 | 转换结果 | 最终状态 | 终端 B 有消息吗 | 实测 |
|---|---|---|---|---|
| FAILURE | | | — | |
| ERROR | | | — | |
| 跳过基类 activate | | | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

- FAILURE：`Transitioning failed`，状态仍是 `unconfigured`，可以再次 configure。
- ERROR：日志 `Callback returned ERROR during the transition: configure`，接着调用 `on_error`。本练习的 `on_error` 返回 SUCCESS，所以最终是 `unconfigured`；如果返回 FAILURE，节点会进入 `finalized`，再也不能用。
- 跳过基类 activate：`ros2 lifecycle get` 显示 **`active [3]`**，但终端 B **一条消息都没有**，`is_activated()` 为 false，终端 A 只有一条 "publisher is not activated" 警告。状态机的状态和发布者的激活状态是两回事。

</details>

## 3. 用 launch 自动拉起

```bash
ros2 launch config_lab lidar_launch.xml
```

<details>
<summary>参考结果</summary>

- 🔍 launch_ros 0.30.3 的 `lifecycle_node` 支持 `autostart="true"`，节点会被自动 configure 再 activate，终端 B 马上有消息。
- 按 Ctrl+C 结束时，日志里出现 `LifecycleNode is not shut down: Node still in state(3) in destructor`：节点在 active 状态下被析构，`on_shutdown` 没有被调用。

</details>

## 挑战（选做）

1. 修改 `on_deactivate`，在停止发布前发一条 `"stopped"`。先调用基类再发，和先发再调用基类，终端 B 分别能不能收到？
2. 在节点里加一个普通订阅 `create_subscription<std_msgs::msg::String>("cmd", ...)`，回调里打印收到的内容。inactive 时 `ros2 topic pub --once /cmd ...`，回调会执行吗？（提示：卡片坑 4）
3. 让定时器在 `on_deactivate` 里 `cancel()`、在 `on_activate` 里 `reset()`，再看第一条消息的编号。
