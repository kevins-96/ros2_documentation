# 练习：控制环为什么被卡住？

> 适用：ROS 2 **Rolling**。对应卡片：[执行器](../cards/executor.md)、[回调组](../cards/callback-group.md)
> 代码：[executor_lab](executor_lab/)（一个 ament_cmake 包，含实验 A、实验 B 和两张卡片的最小示例）
> 用时：约 30 分钟

## 怎么用这份练习

每个实验都分三步：**先写预测 → 再运行 → 对照结果解释差异**。预测错了比预测对了更有价值，错的那一格就是要回卡片重读的地方。

## 0. 编译

```bash
mkdir -p ~/study_ws/src
ln -s <你的仓库路径>/study/labs/executor_lab ~/study_ws/src/executor_lab
cd ~/study_ws
source /opt/ros/rolling/setup.bash
colcon build --packages-select executor_lab
source install/setup.bash
```

没有本机 Rolling 环境时，可以用官方镜像（本练习的结果就是在这个镜像里跑出来的）：

```bash
docker run -it --rm -v <你的仓库路径>/study/labs:/ws/src ros:rolling-ros-base
# 容器内：
source /opt/ros/rolling/setup.bash && cd /ws && colcon build && source install/setup.bash
```

## 实验 A：慢规划服务 vs 50 Hz 控制环

**场景：** 一个节点里有 50 Hz（20 ms）的控制定时器，和一个每次耗时 300 ms 的 `/plan` 服务。程序每 2 秒打印一次控制周期的最大值和"漏拍"次数（间隔超过 30 ms 记一次）。

**运行：**

```bash
# 终端 1：四种配置轮流跑
ros2 run executor_lab blocking_lab --executor single --groups default
# 终端 2：每秒调用 2 次规划服务，制造负载
ros2 service call /plan std_srvs/srv/Trigger -r 2
```

**先填预测**（"最大周期"填大约多少毫秒，"原因"写一句话）：

| `--executor` | `--groups` | 预测最大周期 | 原因 | 实测 |
|---|---|---|---|---|
| single | default | | | |
| single | separate | | | |
| multi | default | | | |
| multi | separate | | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

在 `ros:rolling-ros-base`（rclcpp 33.2.0）里实测，每种配置取 3 个 2 秒窗口：

| `--executor` | `--groups` | 实测最大周期 | 每 2 秒漏拍 |
|---|---|---|---|
| single | default | 306 ~ 310 ms | 4 |
| single | separate | 310 ~ 313 ms | 4 |
| multi | default | 312 ~ 315 ms | 4 |
| multi | separate | 20.1 ~ 21.3 ms | 0 |

解读：
- 每 2 秒 4 次漏拍，正好对应 `-r 2` 下 2 秒内的 4 次规划调用，每次卡住控制环约 300 ms。
- `single + separate`：只有一个线程，分组再细也无法并行。
- `multi + default`：两个回调都在默认互斥组，线程再多也只能轮流跑。
- 只有"多线程 + 不同回调组"两个条件**同时满足**，控制环才不受影响。

</details>

## 实验 B：回调里同步等服务结果

**场景：** 改写自仓库文档 [Using-callback-groups.rst](../../source/ROS-Framework/nodes/Working-with-nodes/Using-callback-groups.rst) 的示例。服务端和客户端放进同一个多线程执行器；客户端每 1 秒在定时器回调里发请求，然后用 `future.wait_for(3s)` 同步等结果。等不到就打印 `TIMEOUT`。

> 与文档原示例的差别：Rolling 的 rclcpp 没有接受 `rmw_qos_profile_t` 的 `create_client` 重载，原示例在 Rolling 上编译失败，这里改成了 `rclcpp::ServicesQoS()`。

```bash
ros2 run executor_lab sync_call_lab --groups same       # 定时器和 client 同在默认组
ros2 run executor_lab sync_call_lab --groups separate   # 各自一个互斥组
ros2 run executor_lab sync_call_lab --groups reentrant  # 共用一个可重入组
```

**先填预测**（每秒都能打印 `Received response`，还是 `TIMEOUT`？）：

| `--groups` | 预测 | 原因 | 实测 |
|---|---|---|---|
| same | | | |
| separate | | | |
| reentrant | | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

| `--groups` | 实测 |
|---|---|
| same | 服务端收到请求并已回复，客户端 3 秒后打印 `TIMEOUT`；之后每 3 秒重复一次 |
| separate | 每秒一次 `Received response`，往返不到 1 ms |
| reentrant | 每秒一次 `Received response` |

注意 `same` 的输出：服务端**已经回复了**，卡住的是客户端这边。响应到达后，要由 client 所在组里的回调把结果写进 future，而那个组正被等待中的定时器回调占着。

</details>

## 挑战（选做，自己动手写）

1. **把它改成你的真实场景。** 把 `blocking_lab` 里的服务换成一个 10 Hz 的点云订阅（回调里 `sleep` 80 ms 模拟滤波），复现并修复控制环抖动。
2. **不用同步等待。** 把 `sync_call_lab` 的定时器回调改成 `async_send_request(request, callback)` 的回调写法，验证 `--groups same` 下也不再超时，并想清楚为什么。
3. **优先级。** 读 `examples_rclcpp_cbg_executor`，用 `add_callback_group` 把控制组放进一个单独的执行器和线程。思考：比起实验 A 的 `multi + separate`，这样做多解决了什么问题？（提示：卡片里的"轮询而非 FIFO"和文档 Outlook 一节里的优先级反转 priority inversion）
