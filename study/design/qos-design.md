---
tags: [ros2, 设计理念, QoS, Deadline, Liveliness, Lifespan]
distro: rolling
status: design-batch-8
aliases: [QoS 的设计, QoS 设计理念, Deadline Liveliness Lifespan, QoS 外部配置]
---

# QoS 的设计：为什么只暴露一部分、Deadline 等策略到底量的是什么

> 适用：ROS 2 **Rolling**（2026-11-23 在 `ros:rolling-ros-base`、rclcpp 33.2.0、`rmw_fastrtps_cpp` 9.6.1 上实测）
> 来源标记：📐 设计文档原文 · 📄 本仓库文档原文 · 🔍 源码/实测核实 · 💡 经验补充
> 设计文档（[ros2/design](https://github.com/ros2/design) @ `93a415b`）：
> [ROS 2 Quality of Service policies](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/qos.md)（2015-10，2019-05 修订）、
> [ROS QoS - Deadline, Liveliness, and Lifespan](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/qos_deadline_liveliness_lifespan.md)（2019-09）、
> [External configurability of QoS policies](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/qos_configurability.md)（2020-11）
> 先读卡片 [QoS](../cards/communication/qos.md)（策略表、兼容规则、overrides 写法），这里只讲"为什么这样设计"和卡片没覆盖的语义细节。

## 1. 设计要回答的问题

三篇文章对应三个阶段的问题 📐：

1. **2015：** 便宜的机器人用不可靠的无线网，ROS 1 只有 TCP。DDS 的 QoS 很细（Topic、DataReader、DataWriter、Publisher、Subscriber 每层都能设），但普通用户选不对。ROS 2 该暴露多少？
2. **2019：** 只能设 History、Depth、Reliability、Durability 四项。想知道"数据是不是按时来"、"上游还活着吗"，就得拿到 rmw 实现去调厂商 API，代码失去可移植性。
3. **2020：** 大家复用别人写的节点，却没有统一的办法改它的 QoS。rosbag2、image_pipeline、Ouster 驱动、Gazebo 插件各自发明了一套参数，用户每换一个包就要重新学。

## 2. 考虑过的方案与取舍

### 暴露哪些、怎么暴露（2015）📐

- **只暴露最常用的几项，再给预设配置（profile）**：默认、服务、传感器数据、参数。预设里的数值"会根据社区反馈调整"。
- **不把"加载厂商 XML"放进 ROS API**，以免 rmw 层绑死 DDS；需要厂商工具配置时，代码里用 `system_default`，把决定权交给厂商的 XML。
- **对非 DDS 的 rmw 也有硬性要求**：没有任何 QoS 会让同一发布者的消息乱序。订阅端取到一条消息后，不能再取到同一发布者更早的消息；Reliable 时新消息要在历史里等旧消息到齐；BestEffort 时取到新的就丢掉旧的。

### Deadline、Liveliness、Lifespan 的几个关键决定（2019）📐

| 决定 | 理由 |
|---|---|
| Deadline **只量到 rmw 层**：rmw 在期限内收到消息就算没超时，不管你的回调多久后才处理 | 简化；"考虑 ROS 开销的 deadline"可以以后另加一个策略 |
| 事件回调**合并**触发：执行器检查时只要有"未处理的状态变化"就调一次，不保证每次违约调一次 | 逐次触发需要额外缓冲，而且 DDS API 本身只告诉你"最新状态"，否则还要求对事件做实时响应；这不是单向门，以后可以改 |
| 话题和服务共用**一个 QoS 结构体** | 改动最小，保持客户端库接口向后兼容 |
| rmw 不支持某个策略时**明确报错** | 宁可起不来，也不要在不知道的情况下以错误的行为运行 |
| 服务和动作**先不支持**这三项 | 语义复杂（例如服务的 Lifespan 相当于请求超时，过期了通知谁？），列为将来的工作 |

### 外部配置 QoS（2020）📐

| 设计目标 | 理由 |
|---|---|
| **节点作者决定**哪些实体可配、什么配置合法（校验回调） | 全部放开会破坏作者的假设，例如作者假设数据是 TransientLocal |
| **不能运行时改** | 改 QoS 往往要销毁实体再重建，期间会丢消息，不适合做成自动机制 |
| 同一个配置可以给多个实体复用 | 两端 QoS 经常需要一致 |
| 只管发布者和订阅者 | 服务和动作的 QoS 还没定义清楚，也不建议改 |
| **用参数，不另造文件格式** | 早期提案用新文件格式，社区反对：又多一套要学的东西，而且 QoS 和"发布频率"这类参数没有本质区别 |

最终优先级 📐：代码里给的 QoS 是默认值 → 被参数覆盖（如果作者允许）→ 仍是 `SYSTEM_DEFAULT` 的项交给 rmw（例如 DDS XML）。

## 3. 设计与 Rolling 现状对照

状态：✅ 按设计实现 · 🔄 实现与设计不同 · ⏳ 未实现 · 🗑️ 已过时。🔍 结果来自[练习](../labs/design-claims-check.md)第 3 部分（`qos_semantics_lab`），每种情形每个节点一个执行器线程。

### 预设配置（`rmw/qos_profiles.h`）🔍

| 预设 | History / Depth | Reliability | Durability | 设计意图 📐 |
|---|---|---|---|---|
| `rmw_qos_profile_default` | KeepLast 10 | Reliable | Volatile | 行为接近 ROS 1，方便迁移 |
| `rmw_qos_profile_services_default` | KeepLast 10 | Reliable | Volatile | **必须 Volatile**：重启的服务端不能收到过期请求 |
| `rmw_qos_profile_sensor_data` | KeepLast **5** | BestEffort | Volatile | 要最新的，不要全部 |
| `rmw_qos_profile_parameters` | KeepLast **1000** | Reliable | Volatile | 参数服务端暂时够不着时请求别丢 |
| `rmw_qos_profile_rosout_default` | KeepLast 1000 | Reliable | **TransientLocal** | （设计文档未列）后启动的日志工具也能拿到之前的日志 |
| `rmw_qos_profile_best_available` | — | — | — | 🔄 设计文档没有：创建时按已发现的端点，在兼容大多数端点的前提下取尽量高的等级 |

### 语义细节

| 设计文档的说法 📐 | 状态 | Rolling 实测 🔍 |
|---|---|---|
| Deadline 只量到 rmw 层，不含回调处理时间 | ✅ | 上游 10 Hz 准时发，两端 deadline 200 ms；订阅回调每条耗时 400 ms。4 s 内只处理了 10 条，消息到回调时最老已经 **1001 ms**，但两端 deadline missed 都是 **0 次** |
| 事件合并触发 | ✅ | 上游 1.0 s 停发，deadline 100 ms，订阅端执行器在 1.2–2.2 s 被另一个回调卡住：卡住期间违约 10 次，恢复后**只回调一次**，`total_count_change=10`；之后每 100 ms 一次、`change=1` |
| Deadline 为 0 表示关闭，默认 0 | 🔄 | rclcpp 的默认值是"未指定"，按无穷大处理（见卡片） |
| Liveliness 有 SYSTEM_DEFAULT / AUTOMATIC / MANUAL_BY_TOPIC（rmw 曾经还有 MANUAL_BY_NODE） | 🔄 | `rclcpp::LivelinessPolicy` 现在是 `Automatic`、`ManualByTopic`、`SystemDefault`、`BestAvailable`、`Unknown`，**没有 ManualByNode** |
| MANUAL_BY_TOPIC：只有发布消息或显式 `assert_liveliness()` 才算活着 | ✅ | 租约 300 ms、每 1 s 发一条：每次发布后 alive，300 ms 后 not_alive，反复切换 |
| AUTOMATIC：存活信号由 rmw 层自动发出 | ✅ 有条件 | 看门狗在**另一个参与者**里：匹配上就 alive（t=0.23 s，一条没发），一直保持；发布者销毁后 alive 变 0。看门狗和发布者在**同一个参与者**里（同一 context，例如同一个组件容器）：直到第一次发布才 alive，**300 ms 后就报 not_alive**，尽管进程好好的 |
| Lifespan：过期的样本从历史里移除，不再发给后来者 | ✅ | TransientLocal + Lifespan 500 ms 发 5 条：0.2 s 后加入的订阅者收到 5 条，1.0 s 后加入的收到 0 条 |
| Lifespan 用"源时间戳 + 时长"算过期，依赖两端时钟同步 | 📐 | 未实测；💡 跨机器用 Lifespan 前先确认 chrony/PTP 同步 |
| 服务和动作支持 Deadline 等 | ⏳ | 未实现 |
| 外部配置：作者用一个选项开启，参数只读，可加校验回调和 id | ✅ | `rclcpp::QosOverridingOptions` 放在 `PublisherOptions`/`SubscriptionOptions` 里（设计文档的示例是单独参数，文中已预告会并入 Options）；`with_default_policies()` 开放 history、depth、reliability，正是设计文档里的"受限默认集合"（见卡片） |
| 外部配置的参数做成"隐藏参数"、YAML 锚点复用配置 | ⏳ | 未实现 💡 |
| 进程内通信和 QoS 怎么结合（2015 年的开放问题） | 🔄 | 已实现，但有限制：第 4 批实测进程内通信 + `KeepAll` 会抛异常（见 [进程内通信](../cards/concurrency/intra-process.md)） |

## 4. 对写代码的影响（规划控制场景）

1. **Deadline 不能当"控制环超时保护"的唯一手段。** 它只保证"消息按时到了 rmw"，不保证"你的控制回调按时处理了"。执行器被卡住时，消息已经过时 1 s，Deadline 仍然一声不吭 🔍。控制器里还要自己比较 `now - msg.header.stamp`，超龄就停车。💡
2. **别用"事件回调被调了几次"来计数。** 用 `info.total_count`（累计）或 `total_count_change`（本次新增），合并触发是设计如此 📐 🔍。
3. **用 Liveliness 做"上游死没死"的看门狗时，看门狗要和被看的节点在不同进程（不同参与者）里。** 放进同一个组件容器后，在 Fast DDS 上 AUTOMATIC 会误报 not_alive 🔍。另一个选择是 MANUAL_BY_TOPIC + 发布方主动 `assert_liveliness()`，语义更清楚：它报告的是"发布方的业务逻辑还在跑"，而不只是"进程还在"。📐 💡
4. **"只要最新目标"的锁存话题加 Lifespan**，避免后启动的节点拿到几分钟前的目标点（TransientLocal 本身不管新旧）🔍。跨机器时注意时钟同步。
5. **写可复用节点时，给传感器订阅、地图发布开 QoS overrides**，用户就不用改你的代码适配他们的网络（见卡片的写法）。但不要开放 durability、liveliness 这类你的逻辑依赖的项。📐
6. **不知道上游用什么 QoS 时**，订阅端可以用 `rclcpp::BestAvailableQoS()`：在"和当前大多数端点兼容"的前提下取尽量高的等级（设计文档之后新增的）。🔍 头文件注释特别提醒：策略在**创建时**就定了，之后发现的端点不兼容也不会再调整，所以和发现过程有竞争、结果可能不确定。💡 启动顺序不固定的系统里慎用，关键链路还是显式写 QoS。

## 5. 与其他概念的关系

- 卡片：[QoS](../cards/communication/qos.md)（策略、兼容表、overrides）· 练习：[QoS 不匹配](../labs/qos-mismatch.md)
- 上一层：[为什么有 ROS 2、为什么建在 DDS 上](why-ros2-dds.md)（为什么是 UDP + QoS）
- [执行器](../cards/concurrency/executor.md)：事件回调也由执行器调度，被卡住时会合并
- [组件与组合](../cards/concurrency/component.md)：合进同一个容器会改变 Liveliness 的行为
- [DDS 调优与 Fast DDS XML](../cards/middleware/dds-tuning.md)：`SYSTEM_DEFAULT` 交给 XML 的那一层
- 练习：[设计文档的说法在 Rolling 里还成立吗](../labs/design-claims-check.md)（第 3 部分）
- 上一级：[总览脑图](../map/overview.md)

## 6. 自测题（先自己答，再看[答案](../answers/qos-design-answers.md)）

1. 底盘驱动订阅 `/cmd_vel`，两端都设了 Deadline 100 ms，并在 deadline 回调里刹车。某天控制器所在机器负载很高，控制器的执行器被路径规划回调卡了 800 ms，这期间 `/cmd_vel` 发布者（在控制器节点里，由定时器发布）也停了。（a）底盘驱动的 deadline 回调会被调用几次？`total_count_change` 大约是多少？（b）如果反过来，是**底盘驱动**自己的执行器被卡了 800 ms，而控制器照常 20 Hz 发布，底盘会刹车吗？此时底盘收到的指令有什么问题？
2. 有人提议："QoS overrides 既然是参数，那就允许 `ros2 param set` 在运行时把激光订阅从 Reliable 改成 BestEffort，省得重启。"按设计文档的理由反驳这个提议；再说出设计文档为什么不让节点用户随意修改所有 QoS 策略。
