# 练习：QoS 不匹配时到底发生了什么？

> 适用：ROS 2 **Rolling**。对应卡片：[QoS](../cards/communication/qos.md)
> 代码：[comm_lab/src/qos_lab.cpp](comm_lab/src/qos_lab.cpp)（依赖 [study_interfaces](study_interfaces/)，一起编译）
> 用时：约 20 分钟

## 0. 编译

和[执行器练习](executor-blocking.md)一样，把 `study/labs/` 下的包放进工作空间的 `src/`：

```bash
ln -s <你的仓库路径>/study/labs/* ~/study_ws/src/
cd ~/study_ws && source /opt/ros/rolling/setup.bash
colcon build --packages-up-to comm_lab && source install/setup.bash
```

## 1. 运行

每种情形运行约 3 秒，结束时打印一行汇总：收到几条消息、两端各触发了几次"不兼容"和"超过截止期"事件。

```bash
ros2 run comm_lab qos_lab --case be-pub-rel-sub
ros2 run comm_lab qos_lab --case rel-pub-be-sub
ros2 run comm_lab qos_lab --case late-volatile
ros2 run comm_lab qos_lab --case late-transient
ros2 run comm_lab qos_lab --case deadline
```

| `--case` | 设置 | 对应的导航场景（💡） |
|---|---|---|
| `be-pub-rel-sub` | 发布 BestEffort，订阅 Reliable，20 Hz | 激光驱动用 SensorDataQoS，你的节点用默认 QoS 订阅 |
| `rel-pub-be-sub` | 发布 Reliable，订阅 BestEffort，20 Hz | 反过来 |
| `late-volatile` | 发布 TransientLocal 只发 1 次，1 s 后创建 Volatile 订阅 | `map_server` 发地图，你的节点后启动 |
| `late-transient` | 同上，订阅也是 TransientLocal | 同上，订阅写对了 |
| `deadline` | 两端都约定 100 ms 截止期，实际只有 5 Hz | 用 Deadline 检测上游掉线 |

## 2. 先填预测

| `--case` | 收到消息？大约几条 | 会触发哪类事件 | 实测 |
|---|---|---|---|
| be-pub-rel-sub | | | |
| rel-pub-be-sub | | | |
| late-volatile | | | |
| late-transient | | | |
| deadline | | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

2026-11-17 在 `ros:rolling-ros-base`（rclcpp 33.2.0，`rmw_fastrtps_cpp`）里实测：

| `--case` | received | 订阅端 incompatible | 订阅端 deadline_missed | 发布端 incompatible | 发布端 deadline_missed |
|---|---|---|---|---|---|
| be-pub-rel-sub | **0** | 1 | 0 | 1 | 0 |
| rel-pub-be-sub | 59 | 0 | 0 | 0 | 0 |
| late-volatile | **0** | 0 | 0 | 0 | 0 |
| late-transient | **1** | 0 | 0 | 0 | 0 |
| deadline | 14 | 0 | 27 | 0 | 27 |

解读：
- `be-pub-rel-sub`：订阅端要求 Reliable，发布端只提供 BestEffort，"请求比提供更严格"，整对不连接；两端各自收到一次 incompatible 事件。**没有注册事件回调的话，你看到的只是"收不到数据"，没有任何报错。**
- `rel-pub-be-sub`：兼容，约 3 秒 × 20 Hz ≈ 60 条。
- `late-volatile`：没有不兼容事件（TransientLocal 提供、Volatile 请求是兼容的），但订阅只拿"之后的新消息"，而发布者再也不发了，所以是 0 条。这一类问题**连事件都没有**，最难查。
- `late-transient`：两端都 TransientLocal，后加入的订阅者拿到了发布者保存的那 1 条。
- `deadline`：消息照常收到（约 15 条），但两端都持续报告截止期错过。具体次数取决于中间件检查的时机，不要依赖精确值。

</details>

## 3. 挑战（选做）

1. 把 `deadline` 情形改成"只有订阅端设 Deadline"，预测并验证结果。（提示：卡片里的兼容表）
2. 给 `qos_lab` 加上 `rclcpp::QosOverridingOptions::with_default_policies()`，用 YAML 参数文件把 `be-pub-rel-sub` 的订阅端改成 BestEffort，不改代码就让它通。
3. 跑 `be-pub-rel-sub` 时另开终端执行 `ros2 topic info -v /lab_topic`，找出输出里能看出问题的那两行。
