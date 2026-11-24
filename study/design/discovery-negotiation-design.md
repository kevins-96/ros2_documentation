---
tags: [ros2, 设计理念, 发现, discovery, 图接口, graph]
distro: rolling
status: design-batch-11
aliases: [发现与协商的设计, Topological Discovery and Communication Negotiation, 计算图与数据层图, computational graph, data layer graph, 图事件, graph event, wait_for_graph_change]
---

# 发现与协商的设计：节点怎么找到彼此、由谁决定怎么连

> 适用：ROS 2 **Rolling**（2026-11-23 在 `ros:rolling-ros-base`、rclcpp 33.2.0、rmw_fastrtps_cpp、Fast DDS 3.6.2 上实测）
> 来源标记：📐 设计文档原文 · 📄 本仓库文档原文 · 🔍 源码/实测核实 · 💡 经验补充
> 设计文档（[ros2/design](https://github.com/ros2/design) @ `93a415b`，本地副本在 `external/ros2_design/articles/`）：
> [Topological Discovery and Communication Negotiation](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/discovery_and_negotiation.md)（2014-06，2019-03 修订）
> 这是 ROS 2 **选 DDS 之前**写的探索性文章，只梳理设计空间和取舍，不给实现方案 📐。读它的价值是：看懂 DDS 替 ROS 2 做了哪些决定。先读卡片 [Domain ID 与发现](../cards/middleware/domain-discovery.md) 和 [为什么 ROS 2 选 DDS](why-ros2-dds.md)。

## 1. 设计要回答的问题

文章区分了两张图 📐：

- **计算图（computational graph）**：节点和它们之间的话题、服务。这是用户写代码时想的那张图。
- **数据层图（data layer graph）**：消息实际怎么送达，包括节点在哪个进程、哪台机器，用 TCP、UDP 组播还是共享内存。

例子：N1、N2 在机器 M1 的进程 P1 里，N3 在 M1 的 P2 里，N4 在 M2 上。它们都收发 T1 📐。计算图很简单，数据层图却可以有很多种：P1 里走内存，P1 和 P2 之间走共享内存，到 M2 走网络。**由谁、根据什么信息来决定数据层图**，就是"协商（negotiation）"。

ROS 1 的做法是一个 master：节点向它登记，它通知订阅者"有新发布者了"，节点再自己建 TCP 连接 📐。文章认为没有一种方案适合所有场景，所以要把接口**分层**，让系统可以按需求"优雅降级" 📐。

## 2. 考虑过的方案与取舍

### 从简单到复杂的七种系统 📐

| 系统 | 远程节点 API | 节点配置 API | 数据层事件 | 动态发现 | 需要图 API |
|---|---|---|---|---|---|
| 静态配置的节点 | | | | | |
| 静态配置的图 | ✓ | | | | |
| 动态配置的图 + 静态发现 | ✓ | ✓ | | | |
| 静态配置的图 + 事件 | ✓ | | ✓ | | ✓ |
| 动态配置的图 + 事件 + 静态发现 | ✓ | ✓ | ✓ | | ✓ |
| 动态配置的图 + 动态发现 | ✓ | ✓ | | ✓ | ✓ |
| 动态配置的图 + 事件 + 动态发现 | ✓ | ✓ | ✓ | ✓ | ✓ |

所有系统都需要最基本的本地节点 API：`connect_to`（建立一条连接，返回句柄）和 `map_to`（把发布者、订阅者映射到某条连接上），外加连接状态的查询 📐。把两者分开，是为了支持 UDP 组播这类"一条连接上跑多个话题"的传输 📐。

逐级增加的能力：

- **静态配置**：节点按一组声明式指令连接，例如"话题 `/foo`、类型 `pkg/Foo`，经 `udpm://225.82.79.83:11311/` 用某种传输和序列化"。这要求先建好发布者、订阅者，再建连接，**隐含了一个生命周期** 📐。
- **远程节点 API**：由一个中心机构远程调用 `connect_to`、`map_to`。
- **节点配置 API**：外部能问节点"你有哪些发布、订阅、服务，支持哪些传输和序列化格式"。节点的唯一标识大概是"机器 id + 进程 id + 节点 id" 📐。
- **数据层事件**：节点向"图"发事件（连接建立 / 断开、心跳、生命周期变化），不用点对点轮询。有了事件就该有**存活性（liveliness）**：节点意外掉线时发不出"我走了"的事件，只能靠心跳超时 📐。
- **动态发现**：节点在运行时加入、离开，地址也是运行时发现的。现有 ROS 的很多工具都依赖这一点 📐。

### 生命周期 📐

要回答"系统起来了没有、连对了没有"，需要每个节点报告自己的状态，例如启动、配置、建立连接、运行、出错、停止。文章特别说：**生命周期报告不应该默认强制**，否则会妨碍用脚本语言调试和做实验 📐。（后来成了 [生命周期的设计](lifecycle-design.md) 里的"受管节点"，是可选的。）

### 图接口的三层 📐

1. **收发层**：向"图"发消息、从"图"收消息。怎么送达由实现决定：有 master 的系统发给 master 转发，无 master 的系统广播给所有节点各自过滤。所有参与者必须事先约定同一种图传输，它是"通用语（lingua franca）"，没有机会协商。
2. **异步事件层**：在收发层上订阅感兴趣的事件，例如"话题 `foo` 有新发布者时通知我"。
3. **图状态层**：维护整张图的状态，回答"谁在发布 `foo`"这类查询。master 用它、无 master 系统的每个节点用它、可视化工具也用它。

### 两个假想的协商系统 📐

- **客户端-服务器 master + 点对点 TCP**：基本就是 ROS 1。
- **分布式 + 智能组播**：每个节点自己维护图状态；本机用 TTL=0 的 UDP 组播，发现另一台机器上有订阅者时，发布者自动开一个 TCP 服务器，对方直接连过来。节点用心跳里的"配置校验和"判断要不要重新拉取对方的配置。

### 留下的开放问题 📐

- **用户能不能给数据层"提示"**，比如"这个话题可以用不可靠传输"？提示可能让节点难以复用；完全不允许提示又不灵活。
- **图通信用什么 RPC、序列化、传输？** 选数据层已有的某一种，还是规定一个必须支持的简单集合？
- **能不能在运行时重映射话题？**

## 3. 设计与 Rolling 现状对照

状态：✅ 按设计实现 · 🔄 实现与设计不同 · ⏳ 未实现 · 🗑️ 已过时。🔍 结果来自[练习](../labs/security-build-design-check.md)第 8 部分（`graph_probe`）和头文件。

| 文章里的概念或问题 | 状态 | Rolling 实测 |
|---|---|---|
| 七种系统里选哪种 | ✅ 选了最完整的一种 | 默认是"动态配置 + 事件 + 动态发现"：DDS 的 SPDP、SEDP 自动发现参与者和端点 📄（见卡片 [Domain ID 与发现](../cards/middleware/domain-discovery.md)）。静态的变体也有：`ROS_AUTOMATIC_DISCOVERY_RANGE=LOCALHOST` + `ROS_STATIC_PEERS`（注意设成 `OFF` 时静态对端也不生效）；中心化的变体是 Fast DDS 的 Discovery Server 📄 |
| 显式的 `connect_to` / `map_to` API | 🔄 | 用户和 rclcpp 都看不到这一层，连接由 DDS 根据发现到的定位器（locator）自己建立 💡 |
| 节点配置 API（有哪些端点、什么类型） | ✅ | `get_publishers_info_by_topic` 返回节点名、类型、QoS、类型哈希 🔍。实测：`node=/graph_pub type=std_msgs/msg/String reliability=reliable durability=transient_local depth=5`，`type_hash version=1` 🔍 |
| 节点唯一标识 = 机器 + 进程 + 节点 | 🔄 | 每个端点有 GID，由 DDS 分配 💡；节点到参与者的对应关系由 rmw 在 DDS 话题 `ros_discovery_info` 上广播，消息是 `ParticipantEntitiesInfo { gid, node_entities_info_seq[] }`，每个节点带命名空间、名字和读写端的 GID 列表 🔍 |
| 节点报告支持的传输，协商器选连接方式 | 🔄 交给 DDS | 由 DDS 实现自动选：Fast DDS 同机走共享内存，跨机走 UDP 🔍（第 7 批抓包）。用户能影响的只有 `FASTDDS_BUILTIN_TRANSPORTS` 这类实现相关的变量 |
| 异步图事件 | ✅ | `node->get_graph_event()` + `wait_for_graph_change()` 🔍。另一个进程创建发布者，**约 2 ms** 后收到图事件 🔍。图事件很频繁：别的进程起一个节点，就会因为参数服务、`rosout` 等端点产生二十来个事件 🔍 |
| 数据层事件（连接建立 / 断开） | ✅ 换了形式 | QoS 事件：`matched_callback`（匹配数变化）、`incompatible_type_callback`、`liveliness_callback`、`deadline_callback` 🔍（`rclcpp/event_handler.hpp`） |
| 存活性：掉线的节点靠心跳超时发现 | ✅ | 发布端正常退出，约 2 ms 后从图里消失；**`kill -9` 后约 19.5 秒**才消失 🔍，和 Fast DDS 参与者默认 20 秒的租期（lease duration）吻合 💡 |
| 图状态查询 | ✅ | `count_publishers`、`get_node_names`、`ros2 node list` 等；CLI 用 daemon 缓存图状态 🔍（第 7 批） |
| 图通信用"数据层已有的一种" | ✅ | 图通信就是 DDS 自己的发现协议，加上一个普通的 DDS 话题 `ros_discovery_info` 🔍 |
| 生命周期报告可选 | ✅ | 普通 `rclcpp::Node` 没有生命周期，`LifecycleNode` 是可选的 🔍（见 [生命周期的设计](lifecycle-design.md)） |
| 用户给数据层的"提示" | ✅ 部分 | QoS（可靠性、历史深度……）就是提示，而且发布者、订阅者两边要兼容 📄；第 10 批的 unique network flows 也是一种提示 🔍 |
| 运行时重映射话题 | ⏳ | 重映射只在节点启动时生效，没有运行时改名的 API 🔍（见 [重映射的设计](remapping-design.md)） |

## 4. 对写代码的影响（规划控制场景）

1. **"等某个节点上线"用图事件，不要用 `sleep`。** 例如规划器启动时等 `/map` 的发布者：`wait_for_graph_change` + `count_publishers`，或者直接用 `transient_local` 的订阅等第一条消息 🔍。💡 要加超时，并且每次醒来都重新检查条件，因为图事件很多而且和你的话题无关。
2. **节点崩溃后，对端要过一个租期才知道。** `kill -9` 后约 20 秒，订阅端的发布者计数才变成 0 🔍。💡 安全相关的监控别靠"发布者消失"，用 DEADLINE 或 LIVELINESS QoS（周期可以设到几百毫秒），或者在控制器里自己检查最新消息的时间戳。
3. **想知道对端用的 QoS、类型版本，用 `get_publishers_info_by_topic`。** 🔍 它还给出类型哈希，排查"类型不兼容收不到"时比猜快。`ros2 topic info -v` 显示的也是这些信息。
4. **别指望控制"走哪条网卡、用什么传输"成为可移植的代码。** 这一层在 DDS 里，而且各家不同 🔄。💡 写在部署配置（XML 配置文件、环境变量）里，不写进节点代码。
5. **把 `ros_discovery_info` 当成基础设施。** 🔍 它是 rmw 内部用的 DDS 话题，SROS2 的权限模板会自动放行它（见[访问控制策略的设计](access-control-design.md)）。手写 DDS 权限文件时漏了它，节点在 `Enforce` 下起不来（第 7 批换 Domain 的实验报的就是 `ros_discovery_info`）。

## 5. 与其他概念的关系

- 卡片：[Domain ID 与发现](../cards/middleware/domain-discovery.md) · [中间件 RMW 与 DDS](../cards/middleware/rmw-dds.md) · [QoS](../cards/communication/qos.md) · [命令行工具](../cards/observability/cli-tools.md)（daemon）
- 练习：[中间件、Domain 与发现](../labs/middleware-discovery.md) · [安全与工程化的设计还成立吗](../labs/security-build-design-check.md)（第 8 部分）
- [为什么 ROS 2 选 DDS](why-ros2-dds.md)：这篇文章之后的决定
- [ROS 图怎么映射到 DDS](graph-to-dds-mapping.md)：节点怎么对应到参与者
- [QoS 的设计](qos-design.md)：LIVELINESS、DEADLINE
- [生命周期的设计](lifecycle-design.md) · [重映射的设计](remapping-design.md)
- 上一级：[总览脑图](../map/overview.md)

## 6. 自测题（先自己答，再看[答案](../answers/discovery-negotiation-design-answers.md)）

1. 你的局部规划器订阅 `/odom`，要求里程计断了 200 ms 以内就停车。同事的方案是："订阅一个图事件，`count_publishers("/odom")` 变成 0 就停车。"（a）根据本篇的实测，里程计节点崩溃（相当于 `kill -9`）时，这个方案多久才会触发？（b）为什么会这么慢？（c）给出一个能满足 200 ms 的方案，说明用到哪个 QoS 或什么代码逻辑。
2. 把 Rolling 的 ROS 2 放进文章的七种系统里：（a）默认配置属于哪一种？（b）设 `ROS_AUTOMATIC_DISCOVERY_RANGE=LOCALHOST` 并用 `ROS_STATIC_PEERS` 列出另一台机器的地址时，更接近哪一种？少了什么能力？（c）文章里"节点报告自己支持哪些传输，由协商器决定怎么连"在 ROS 2 里由谁来做？这样设计的好处和代价各是什么？
