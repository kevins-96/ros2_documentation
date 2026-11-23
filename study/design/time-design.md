---
tags: [ros2, 设计理念, 时间, 仿真时间, use_sim_time]
distro: rolling
status: design-batch-9
aliases: [时钟与时间的设计, Clock and Time design, ROS 时间, 仿真时间设计, 时间跳变]
---

# 时钟与时间的设计：三种时钟、`/clock`、时间跳变

> 适用：ROS 2 **Rolling**（2026-11-23 在 `ros:rolling-ros-base`、rclcpp 33.2.0、rcl 10.6.0 上实测）
> 来源标记：📐 设计文档原文 · 📄 本仓库文档原文 · 🔍 源码/实测核实 · 💡 经验补充
> 设计文档（[ros2/design](https://github.com/ros2/design) @ `93a415b`，本地副本在 `external/ros2_design/articles/130_ros_time.md`）：
> [Clock and Time](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/130_ros_time.md)（原文标注 2018-07 写成、2015-12 修订，日期前后颠倒）
> 相关卡片：[tf2 与时间](../cards/coordinates/tf2-time.md)（查询变换时的时间参数）、[日志](../cards/configuration/logging.md)（节流宏用节点时钟）、[rosbag 录制回放](../cards/observability/rosbag.md)（`--clock` 回放）。

## 1. 设计要回答的问题

很多机器人算法依赖时间和同步，所以要求网络里各台机器的系统时钟是同步的 📐。但有些场合需要**控制时间的推进** 📐：

- **回放日志**：加速、减速、单步，暂停在某个时刻细查。光回放传感器数据不够，系统其他部分的时间和数据对不上，很多算法就会出错。
- **跑仿真**：仿真可能比真实时间快（批量测试），也可能慢（复杂系统要精度不要速度）。仿真器是最慢的一环，所以由它做时间源；仿真暂停，整个系统也要能跟着暂停。

这带来几个难点 📐：时间可能暂停，不能假设它总在增长；时间的变化要通过网络通知各节点，会有延迟，而且实时倍率越高误差越大；回放还会让时间**往回跳**，就像系统时钟被往回调了一样。

## 2. 考虑过的方案与取舍

| 决定 | 理由 📐 |
|---|---|
| 三种时钟：`SystemTime`、`SteadyTime`、`ROSTime`，对应 `std::chrono` 的 `system_clock`、`steady_clock` | 默认用 ROS 时间；和硬件打交道需要超时的地方用 steady；需要真实墙钟时用 system |
| `ROSTime` 没有激活时等于系统时间；参数 `use_sim_time` 打开时，改为返回 `/clock` 话题上最新收到的时间 | 和 ROS 1 一样，由一个时间源发布 `/clock` |
| 收到 `/clock` 之前返回 **0**，0 表示"时间还没初始化" | 让用户能判断出这种情况 |
| **不做外推估计**：两次 `/clock` 之间时间不动 | 外推要假设时间将来连续推进，暂停一来就全错了。想更精确就降低实时倍率或提高 `/clock` 频率 |
| 时间往回跳时，先调用注册的回调，**回调完成之后**才报告新时间 | 给用户机会在时间回到过去之前清掉状态（例如 tf 缓存） |
| 跳变回调可以设阈值：向前多少、向后多少、是否包括时钟类型变化 | 不是每次小变化都要通知 |
| `SteadyTime` 和另外两种时间**类型不同**，编译期就不能混用；`SystemTime` 和 `ROSTime` 共用一个基类，运行时检查能否比较 | 在编译型语言里，误用能在编译时发现 |
| 阻塞类 API（sleep）要提供参数，决定遇到时间跳变时是立刻报错还是忽略 | 用户自己选择 |
| GPS 这类外部时间源建议通过 NTP 接到系统时钟，不要做成 ROS 时间源 | NTP 是成熟方案，也不用处理时间跳变 |

## 3. 设计与 Rolling 现状对照

状态：✅ 按设计实现 · 🔄 实现与设计不同 · ⏳ 未实现 · 🗑️ 已过时。🔍 结果来自[练习](../labs/config-design-check.md)第 4 部分（`time_design_probe`：同一进程里一个 `use_sim_time=true` 的节点和一个自己发 `/clock` 的节点）。

| 设计文档的说法 | 状态 | Rolling 实测 |
|---|---|---|
| `use_sim_time` 打开后 ROS 时间来自 `/clock` | ✅ | 时钟类型 `RCL_ROS_TIME`，`ros_time_is_active()=1` 🔍 |
| 还没收到 `/clock` 时返回 0 | ✅ | `now() = 0.000` 🔍。rclcpp 还提供了 `Clock::wait_until_started()`，用来等时间变成非零 💡 |
| 不外推，两次 `/clock` 之间时间不动 | ✅ | `/clock` 停发 2 s（墙钟），ROS 定时器（0.5 s）触发 **0** 次，墙钟定时器触发 4 次 🔍 |
| 时间按 `/clock` 的速度推进 | ✅ | 1 s 墙钟内仿真走了 10 s：ROS 定时器触发 20 次，墙钟定时器 2 次 🔍 |
| 往回跳时先调回调，再报告新时间 | ✅ | 从 100 s 跳回 40 s：跳变前回调里 `now()` 仍是 100.000，跳变后回调报 `delta = -60.000 s`，此时 `now()` 已是 40.000 🔍 |
| 跳变回调可以设阈值 | ✅ | `rcl_jump_threshold_t` 有 `min_forward`、`min_backward`、`on_clock_change` 三项 🔍。如果 `min_forward` 设得很小，**每一次 `/clock` 更新**都会触发"向前跳变"（实测 100 次更新触发 100 次）🔍 |
| 运行时切换 `use_sim_time` 会通知 | ✅ | 改成 `false` 后，回调报"ROS 时间被关闭"，`now()` 变回墙钟 🔍 |
| `SteadyTime` 与其他时间编译期就不能混用 | 🔄 | 三种时间都是同一个类型 `rclcpp::Time`，只在运行时区分。`steady < ros` **能编译**，运行时抛 `can't compare times with different time sources` 🔍 |
| `SystemTime` 和 `ROSTime` 可以互相比较 | 🔄 | 同样抛异常 🔍（即使 ROS 时间没有激活、实际值等于系统时间） |
| 提供 `Time`、`Duration`、`Rate`，三种时钟都能用 | 🔄 `Rate` 的默认值不对 | `rclcpp::Rate(10)` 的默认时钟是 **SYSTEM**，不跟仿真时间走；要写 `rclcpp::Rate(10, node->get_clock())` 🔍 |
| `/clock` 的频率和粒度由应用决定 | ✅ | rclcpp 订阅 `/clock` 用的是 `ClockQoS`：depth 1、**best_effort** 🔍。只要最新值，丢几条无所谓 |
| 自定义时间源，可以在代码里替换 | ✅ 部分 | `rclcpp::TimeSource` 可以挂到别的时钟上；没有"用参数选择时间源"这种机制 💡 |

## 4. 对写代码的影响（规划控制场景）

1. **算法里的时间一律用节点时钟**（`now()`、`create_timer`、`Rate(hz, get_clock())`），不要用 `std::chrono::system_clock`、`create_wall_timer` 和默认构造的 `rclcpp::Rate` 🔍。否则回放和仿真时，控制周期、超时判断、tf 查询的时间都和数据对不上。
2. **只有和硬件打交道的超时用 steady**，例如串口读写超时、看门狗喂狗。这类代码要和 ROS 时间隔离开，对外只给 ROS 时间戳 📐。
3. **启动时处理"时间为 0"**：`use_sim_time=true` 时，`/clock` 到来之前 `now()` 是 0 🔍。这时算出的时间差、发出的时间戳都是错的。在构造或 configure 里先等 `wait_until_started()`，或者跳过时间为 0 的周期。💡
4. **回放循环播放、`ros2 bag play --loop` 或仿真重置时，时间会往回跳** 📐 🔍。用 `create_jump_callback` 注册回调，在回调里清空 tf 缓存、滤波器状态、轨迹时间索引（tf2 的 `Buffer` 自己会处理往回跳：日志 `Detected jump back in time. Clearing TF buffer.` 🔍）。阈值要合理：向前只关心大跳（例如 1 s 以上），向后的跳变任意大小都要报 🔍。
5. **别在仿真时间里混用 steady 和 ROS 时间做比较**，编译器不会拦你，运行时才抛异常 🔍。例如用 `steady_clock_->now()` 和消息时间戳相减。
6. **`/clock` 是 best effort**：网络差时会丢，丢了不补 🔍。仿真机和算法机分开时，注意这条链路的带宽和延迟；高实时倍率下时间误差会被放大 📐。

## 5. 与其他概念的关系

- [tf2 与时间](../cards/coordinates/tf2-time.md)：`use_sim_time` 没设对时 tf 查询全部失败
- [日志](../cards/configuration/logging.md)：节流宏用节点时钟，仿真暂停时节流期不结束
- [rosbag 录制回放](../cards/observability/rosbag.md)：`ros2 bag play --clock` 由回放器发布 `/clock`
- [参数的设计](parameter-design.md)：`use_sim_time` 就是一个普通参数
- [执行器](../cards/concurrency/executor.md)：ROS 时钟定时器的唤醒也由执行器调度
- 练习：[配置与启停的设计还成立吗](../labs/config-design-check.md)（第 4 部分）
- 上一级：[总览脑图](../map/overview.md)

## 6. 自测题（先自己答，再看[答案](../answers/time-design-answers.md)）

1. 一个 20 Hz 的路径跟踪控制器这样写主循环：`rclcpp::Rate rate(20); while (rclcpp::ok()) { auto t = now(); ...; rate.sleep(); }`，用 `ros2 bag play --clock -r 0.5`（半速回放）测试。（a）每个控制周期在仿真时间里是多长？控制器"以为"自己是多少 Hz？（b）回放暂停时会怎样？（c）怎么改？
2. 回放一个 60 s 的包并用 `--loop` 循环。第二圈开始后，定位节点不断报 tf 外推错误，速度平滑器算出的加速度出现了一个巨大的尖峰。（a）用设计文档的概念解释发生了什么。（b）设计文档提供了什么机制来处理？在 Rolling 里怎么用？阈值怎么设？（c）有人想用 steady 时钟算控制周期的 dt 来"避开时间跳变"，这样做在回放时会有什么问题？
