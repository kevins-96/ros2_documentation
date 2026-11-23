---
tags: [ros2, 设计理念, DDS, 名字, 参与者]
distro: rolling
status: design-batch-8
aliases: [ROS 图到 DDS 的映射, 话题名映射, 节点与参与者, rt 前缀, Node to Participant mapping, Topic and Service name mapping]
---

# ROS 图怎么映射到 DDS：名字规则、`rt/` 前缀、节点与参与者

> 适用：ROS 2 **Rolling**（2026-11-23 在 `ros:rolling-ros-base`、rclcpp 33.2.0、rcl 10.6.0、`rmw_fastrtps_cpp` 9.6.1 上实测）
> 来源标记：📐 设计文档原文 · 📄 本仓库文档原文 · 🔍 源码/实测核实 · 💡 经验补充
> 设计文档（[ros2/design](https://github.com/ros2/design) @ `93a415b`）：
> [Topic and Service name mapping to DDS](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/140_topic_and_service_name_mapping.md)（2016-10，2018-06 修订）、
> [Node to Participant mapping](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/170_node_to_participant_mapping.md)（2020-06，Foxy 起生效）

## 1. 设计要回答的问题

ROS 的概念和 DDS 的概念对不上 📐：

| ROS 这边 | DDS 这边 | 冲突 |
|---|---|---|
| 话题名有 `/` 分层、`~` 私有名、`{}` 替换 | DDS 话题名只允许字母、数字、下划线（后来厂商放开了 `/`） | 字符集不同 |
| 服务、动作、参数都有名字 | DDS 只有话题（DDS-RPC 当时没定稿） | 需要约定"哪个 DDS 话题属于哪个服务" |
| 名字可以很长（命名空间嵌套） | RTPS 规定话题名最长 256 字符 | 要省着用前缀 |
| 节点是轻量的分组单位，一个进程可以有很多 | DDS 参与者（DomainParticipant）是重量级实体：每个都参与发现、有自己的线程和缓存 | 一个节点一个参与者太贵 |

## 2. 考虑过的方案与取舍

### 名字：怎么把 `/` 塞进 DDS 📐

| 方案 | 做法 | 为什么没选 |
|---|---|---|
| 用 DDS 分区（partition）装命名空间 | `/camera/data` → 分区 `camera` + 话题 `data` | 把分区挪作他用；Connext 上 `/camera/data`（Image）和 `/imu/data`（Imu）同为 `data` 却类型不同，出错；DDS-XRCE 这类新标准可能没有分区 |
| 把 `/` 换成 `__` | `/foo/bar` → `rt__foo__bar` | 每层命名空间多吃 1 个字符；替换后还要重做长度校验 |
| 用大写字母代替 `/` | 禁止名字里出现大写 | 限制太大，和 ROS 1 不兼容 |
| 普通话题不加前缀 | `image` → `image` | 方便和原生 DDS 程序互通，但分不清哪些 DDS 话题是 ROS 建的 |
| 后缀而不是前缀 | `foo` → `foo_rt_` | 用 DDS 工具列话题时不能按前缀归类；厂商的 Request-Reply 实现还会再加后缀，容易混淆 |
| **最终：厂商放开了 `/`，于是"前缀 + 完整名字"** | `/foo/bar` → `rt/foo/bar` | 简单直观；代价是完整名字和前缀一起受长度限制 |

### 节点：一个节点一个参与者，还是共用 📐

| 方案 | 好处 | 代价 |
|---|---|---|
| Foxy 之前：一个节点一个参与者 | 实现简单，DDS 参与者的功能正好对应节点 | 发现流量、CPU、线程、内存随节点数线性增长；Connext 一个 Domain 最多 120 个参与者 |
| 一个进程一个参与者 | 最省 | 不灵活，例如 domain bridge 需要一个进程里有多个参与者 |
| **最终：一个 context 一个参与者** | 绝大多数程序只有一个 context，等于一个进程一个；需要时可以多建 context | 节点信息不再能从参与者本身读出，要额外传 |

"哪个读写端属于哪个节点"怎么传？选的是一个内部话题 `ros_discovery_info`：每个参与者发布一条消息，内容是参与者 GID 和它承载的每个节点的命名空间、名字、读端和写端 GID；有实体增减就重发。写端 Reliable + TransientLocal + KeepLast 1，读端 KeepAll。备选方案"用参与者/读写端的 USER_DATA 携带节点信息"在安全上更好（不需要一个所有节点都能读写的话题），但当时有厂商不支持。这个话题是**实现细节**，放在 `rmw_dds_common` 里，不进 `rcl` 的 API。📐

## 3. 设计与 Rolling 现状对照

状态：✅ 按设计实现 · 🔄 实现与设计不同 · ⏳ 未实现 · 🗑️ 已过时。🔍 结果来自[练习](../labs/design-claims-check.md)第 1、2 部分。

### 名字规则

| 设计文档的规则 📐 | 状态 | Rolling 实测 🔍 |
|---|---|---|
| 相对名接在节点命名空间后；`/` 开头是绝对名 | ✅ | `plan` → `/robot1/plan`，`/global_plan` → `/global_plan` |
| `~` 展开为"命名空间/节点名"，必须写成 `~/xxx` | ✅ | `~/debug` → `/robot1/planner/debug`；`~foo` 报 `tilde '~' that is not followed by a forward slash` |
| `{}` 替换；设计文档**没规定**有哪些替换 | ✅ | rcl 定义了三个：`{node}`、`{ns}`、`{namespace}`（`rcl/expand_topic_name.h`）。`{node}/status` → `/robot1/planner/status` |
| 不能以数字开头、不能以 `/` 结尾、不能有 `//` | ✅ | 三种都被拒绝 |
| 不能有连续下划线 `__` | 🔄 | `foo__bar` **被接受**，展开成 `/robot1/foo__bar` |
| 以 `_` 开头的段是隐藏名字，工具默认不显示 | ✅ | 动作的 `_action/...` 就靠这个隐藏 |
| 可以写成 URL：`rostopic:///foo`、`rosservice:///foo` | ⏳ | `rostopic:///foo` 报 `must not contain characters other than alphanumerics...` |
| 包括前缀在内不超过 256 字符 | 🔄 | 限制是**完整 ROS 名字 ≤ 247**（`RMW_TOPIC_MAX_NAME_LENGTH = 255U - 8U`，8 个字符留给前缀）：`/robot1/` + 239 个字符可以，240 个被拒 |

### 前缀与类型名

| 设计文档 📐 | 状态 | Rolling 实测（抓 RTPS 发现报文）🔍 |
|---|---|---|
| 话题 `rt/` | ✅ | `rt/robot1/plan`、`rt/rosout`、`rt/parameter_events` |
| 服务用一对话题时：请求 `rq/`、响应 `rr/` | ✅ 加后缀 | `rq/robot1/computeRequest`、`rr/robot1/computeReply` |
| 服务 `rs/`、参数 `rp/`、动作 `ra/` | 🗑️ 没用上 | 参数服务就是普通服务：`rq/robot1/planner/get_parametersRequest`；动作是 `rt/.../_action/feedback`、`rt/.../_action/status` 加 `rq/.../_action/send_goalRequest` 等，**没有 `ra/`** |
| 类型名（设计文档未细说） | — | `std_msgs::msg::dds_::String_`、`study_interfaces::srv::dds_::ComputePath_Request_`，规律是 `包::msg|srv|action::dds_::类型_` |
| `avoid_ros_namespace_conventions=true` 时不加前缀：`rostopic://image` → DDS 话题 `image` | 🔄 | 只去掉了 `rt`，**命名空间照样展开、开头的 `/` 也保留**：`raw_dds_topic` → DDS 话题 `/robot1/raw_dds_topic` |

### 节点与参与者

| 设计文档 📐 | 状态 | Rolling 实测 🔍 |
|---|---|---|
| 一个 context 一个参与者 | ✅ | 同进程两个节点：`/robot1/planner`、`/robot1/controller` 的 `/rosout` 写端 GUID 前缀相同（同一参与者）；加 `--separate-context` 后变成两个参与者 |
| 节点信息走内部话题 `ros_discovery_info` | ✅ | 抓包能看到 DDS 话题 `ros_discovery_info`、类型 `rmw_dds_common::msg::dds_::ParticipantEntitiesInfo_`。但 rclcpp **订阅不到**它：即使 `avoid_ros_namespace_conventions`，名字也会变成 `/ros_discovery_info` |
| 安全凭据从"每个节点"变成"每个进程"（enclave） | ✅ | 见 [安全 SROS2](../cards/middleware/sros2-security.md) |
| `ignore_local_publications` 以后实现起来"不那么直接" | 🔄 语义变了 | 订阅设了 `ignore_local_publications=true`：同节点发的 10 条收到 0；**同进程另一个节点**发的 10 条也收到 0；另一个进程发的照常收到。改成 `--separate-context` 后，另一个节点的 10 条全收到。也就是说"本地"指的是**同一个参与者**，不是同一个节点 |

```mermaid
flowchart LR
  subgraph P["进程 / context（一个 DDS 参与者）"]
    N1["节点 /robot1/planner"] --> W1["写端 rt/robot1/plan"]
    N1 --> S1["读写端 rq/robot1/computeRequest<br/>rr/robot1/computeReply"]
    N2["节点 /robot1/controller"] --> W2["写端 rt/robot1/cmd_vel"]
    RD["写端 ros_discovery_info<br/>（参与者 GID → 节点 → 读写端 GID）"]
  end
  RD -. 其他参与者据此还原"哪个端点属于哪个节点" .-> X[ros2 node info / 图 API]
```

## 4. 对写代码的影响（规划控制场景）

1. **可复用节点里别写绝对名。** 写 `plan`、`~/debug`，不写 `/plan`。多机器人时给每台车一个命名空间（`/robot1`），相对名自动隔离；绝对名会让所有车挤在同一个话题上。📐 💡
2. **名字长度有预算。** 完整名字 ≤ 247 字符 🔍。`/fleet/site_a/robot_12/perception/...` 这种层层嵌套在 launch 里拼出来时容易超；超了是创建时抛异常，不是静默截断。
3. **手写 SROS2 权限、用 Wireshark 或 DDS 厂商工具时，看的是 DDS 名字**：话题 `rt/...`，服务 `rq/...Request`、`rr/...Reply`，参数服务跟着节点名走（见 [安全 SROS2](../cards/middleware/sros2-security.md) 常见坑 5）。🔍
4. **一个进程里多建几个节点很便宜**，它们共用一个参与者；但别为每个节点建一个 context，那等于退回 Foxy 之前的"一个节点一个参与者"。🔍 💡
5. **把节点合进同一个组件容器时，`ignore_local_publications` 的效果会变。** 分开运行时只忽略自己发的；合进一个进程后，同进程所有节点发的都被忽略 🔍。用它做"回环抑制"的代码（例如一个节点既发又收 `/tf`、只想过滤自己）在组合部署后可能什么都收不到。💡 想过滤自己，更稳的做法是在消息里带来源 ID（例如 `header.frame_id` 或自定义字段）自己判断。
6. **和非 ROS 的 DDS 程序互通**：原生 DDS 程序要用 `/xxx` 这样带 `/` 的话题名（或者干脆按 ROS 规则用 `rt/xxx`），类型名也要对上 `包::msg::dds_::类型_` 🔍。

## 5. 与其他概念的关系

- 上一层：[为什么有 ROS 2、为什么建在 DDS 上](why-ros2-dds.md)
- 卡片：[节点与 ROS 图](../cards/communication/node.md)、[启动参数与重映射](../cards/configuration/node-arguments.md)（重映射改的就是这些名字）、[Domain ID 与发现](../cards/middleware/domain-discovery.md)（参与者编号与端口）、[安全 SROS2](../cards/middleware/sros2-security.md)、[组件与组合](../cards/concurrency/component.md)
- 动作的名字展开：[服务与动作的设计](service-action-design.md)
- 练习：[设计文档的说法在 Rolling 里还成立吗](../labs/design-claims-check.md)（第 1、2 部分）
- 上一级：[总览脑图](../map/overview.md)

## 6. 自测题（先自己答，再看[答案](../answers/graph-to-dds-mapping-answers.md)）

1. 节点 `local_planner` 在命名空间 `/robot2` 里，代码里创建了：发布者 `"cmd_vel"`、发布者 `"~/costmap"`、服务 `"clear"`、动作服务端 `"follow_path"`。写出：（a）每个的完整 ROS 名字；（b）在 Fast DDS 抓包里会看到的 DDS 话题名（动作只写出两个话题和 `send_goal` 服务）；（c）`ros2 topic list` 默认会显示哪些。
2. 你的里程计融合节点 `fuser` 订阅 `/odom` 时设了 `ignore_local_publications=true`，因为它自己也会往 `/odom` 发修正后的结果，想避免收到自己的消息。原来每个节点单独一个进程，一切正常。上车时为了省拷贝，把 `fuser` 和轮速里程计节点 `wheel_odom`（也发 `/odom`）放进了同一个组件容器。会发生什么？为什么？怎么改？
