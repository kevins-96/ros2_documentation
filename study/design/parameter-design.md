---
tags: [ros2, 设计理念, 参数]
distro: rolling
status: design-batch-9
aliases: [参数的设计, 参数设计理念, Parameter API design, 为什么没有参数服务器]
---

# 参数的设计：为什么参数挂在节点上、为什么要先声明

> 适用：ROS 2 **Rolling**（2026-11-23 在 `ros:rolling-ros-base`、rclcpp 33.2.0、rcl 10.6.0 上实测）
> 来源标记：📐 设计文档原文 · 📄 本仓库文档原文 · 🔍 源码/实测核实 · 💡 经验补充
> 设计文档（[ros2/design](https://github.com/ros2/design) @ `93a415b`，本地副本在 `external/ros2_design/articles/`）：
> [Parameter API design in ROS](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/055_ros_parameter_design.md)（2015-09，2019-05 修订）、
> [ROS Command Line Arguments](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/160_ros_command_line_arguments.md)（参数部分，2019-09，2021-08 修订）
> 先读卡片 [参数](../cards/configuration/parameter.md)（声明、回调、YAML 写法），这里讲"为什么这样设计"和卡片没覆盖的规则。

## 1. 设计要回答的问题

ROS 1 有两套参数 📐：

- **参数服务器（parameter server）**：一块全局"黑板"（blackboard），任何节点都能读写任何参数，没有所有者，也没人校验。改了值，用它的节点也不知道。
- **dynamic_reconfigure**：为了"运行时调驱动参数"另做的一套，通过服务和节点交互，节点可以拒绝。

黑板模型的毛病在于没有所有权 📐：谁都能写，没人负责校验，参数的寿命也说不清，删掉之前一直留在那。名字还有歧义：`/foo/bar/baz` 可能是节点 `/foo/bar/baz`，也可能是节点 `/foo/bar` 的私有参数 `baz`。

设计文档列出的"理想系统"要能做到这些 📐：
- 读、写、删除参数，并且能成组地原子完成
- 列出参数（数量多时只列一部分）
- 参数变化时发通知
- 能拒绝修改，并把约束（例如整数范围）告诉外部工具，方便自动生成界面
- 不真正修改就能预先知道某个值会不会被接受
- 参数的寿命规则清楚
- 名字无歧义
- 能记录下来，供回放和分析

## 2. 考虑过的方案与取舍

| 决定 | 理由 📐 |
|---|---|
| **参数挂在节点上**，寿命跟着节点走 | 由节点负责校验，寿命规则就简单了：节点在，参数在。需要持久化时由节点自己实现 |
| 参数用"节点全名 + 参数名"两部分寻址 | 消除 ROS 1 的名字歧义 |
| 值的类型只有 `bool`、`int64`、`float64`、`string`、`byte[]`、`bool[]`、`int64[]`、`float64[]`、`string[]` | 不提供 8/16/32 位和无符号类型，这样从文本配置文件读进来时不会有歧义。数组用来表示向量和矩阵，机器人里很常见。只支持同类型数组，免得引入多维、异构数组的内省 |
| 每个节点提供 get、set、list、describe 四类服务 | 本地 API 和远程服务调用用同一套语义 |
| set 可以原子地设一批，任何一个校验失败就**全部不生效** | 相关参数（例如最大速度和最大加速度）要一起改 |
| **校验要快**，只回答"接受还是拒绝" | 不要在校验里考虑"这个值对系统性能有什么影响" |
| 参数变化发到一个话题上 | 用来监控，也能用普通的录包功能记录参数变化，回放时再调 set |
| 兼容 ROS 1：写一个"接受一切修改"的节点放在 `/`，模拟参数服务器 | 需要"不属于任何节点"或"比节点活得久"的参数时用 |
| `searchParam`（沿命名空间向上找参数）可以通过按层级遍历节点实现 | ROS 1 用户习惯这种用法 |

当时没想清楚、留作将来的两件事 📐：
- **参数怎么在启动时加载**，留给 launch 系统一起解决；后来由命令行参数设计文档定下来，就是现在的 `-p` 和 `--params-file`。
- **"预先声明接口"**：让节点声明自己有哪些参数，开发工具就能静态检查，拼错参数名的问题也能提前发现。

## 3. 设计与 Rolling 现状对照

状态：✅ 按设计实现 · 🔄 实现与设计不同 · ⏳ 未实现 · 🗑️ 已过时。🔍 结果来自[练习](../labs/config-design-check.md)第 2 部分（`param_design_probe`、`remap_probe`）。

| 设计文档的说法 | 状态 | Rolling 实测 / 源码 |
|---|---|---|
| 参数挂在节点上，寿命跟着节点 | ✅ | 见卡片坑 9：`ros2 param set` 的修改重启就没了 |
| "预先声明接口"留作将来 | ✅ 已实现，而且是默认 | 默认 `allow_undeclared_parameters=false`：本地 `set_parameter("max_jerk")` 抛异常 `parameter 'max_jerk' cannot be set because it was not declared` 🔍。拼错名字在设置时就被发现 |
| 每个节点有 get、set、list、describe | ✅ 还多了几个 | `~/describe_parameters`、`~/get_parameter_types`、`~/get_parameters`、`~/list_parameters`、`~/set_parameters`、`~/set_parameters_atomically` 🔍 |
| 原子批量设置，一个失败全部不生效 | ✅ | `set_parameters_atomically({max_speed=1.5, max_accel=2.0})` 被校验回调拒绝后，两个值都没变；普通的 `set_parameters` 则**逐个**处理：max_speed 成功，max_accel 被拒 🔍。命令行对应 `ros2 param set 节点 名1 值1 名2 值2 --atomic` 🔍 |
| "设置前校验的回调还没实现"（2019 的 Unimplemented） | ✅ 已实现 | `add_on_set_parameters_callback`，还有 pre-set 和 post-set 回调（见卡片） |
| 把约束告诉外部工具 | ✅ 部分实现 | 描述里的 `floating_point_range` / `integer_range` 会被**自动执行**：设 3.0 给范围 [0, 2] 的参数，返回 `doesn't comply with floating point range` 🔍；`describe_parameters` 能读到范围和说明。跨参数的约束（"加速度小于速度"）只能写在回调里，外部看不见 |
| 不修改就能预知会不会被接受 | ⏳ | 没有这类 API；只能真的 set 一次看结果 💡 |
| 订阅**某一个**参数的变化（2019 的 Unimplemented） | ✅ 已实现 | `rclcpp::ParameterEventHandler::add_parameter_callback("max_speed", ...)` 🔍 |
| **每个节点**提供一个参数事件话题 | 🔄 | 只有**一个全局话题** `/parameter_events`，所有节点都往里发，消息里用 `node` 字段区分 🔍。被拒绝的修改不发事件；声明参数时发一条"新增"事件 🔍 |
| 参数变化可以录包并回放 | ⏳ 只实现了一半 | `/parameter_events` 是普通话题，可以录；没有"回放时自动调 set"的工具 💡 |
| 模拟 ROS 1 的全局参数服务器 | 🔄 只在 demo 里 | 核心库没有。`demo_nodes_cpp` 里有一个 `parameter_blackboard`，接受任意参数 📄（`Releases/Release-Galactic-Geochelone.rst`），`ros-base` 镜像里没有装 🔍 |
| `searchParam` | ⏳ | 没有 |
| 类型只有 64 位整数、双精度和它们的数组，外加 `byte[]` | ✅ | `blob`（`byte[]`）、`mask`（`bool[]`）都能声明 🔍。整数只有 `int64`，所以 YAML 里写 `1` 和 `1.0` 是两种类型（见卡片坑 1） |
| 参数名的层级 | 🔄 设计没定 | 嵌套参数用 `.` 分隔（`controller.kp`），不用 `/`；`list_parameters({"controller"})` 按前缀列出 🔍。这正好避开了 ROS 1"斜杠既分隔节点又分隔参数"的歧义 |
| `-p 节点名:参数:=值` 只设给某个节点（160） | 🔄 只对根命名空间里的节点有效 | 节点在 `/robot1` 下时，`-p planner:max_speed:=1.5` **静默无效**；写成 `-p /robot1/planner:max_speed:=1.5` 会解析报错 🔍。根命名空间里的 `/planner` 能收到。要精确指定命名空间里的节点，只能用 YAML 的全名键 |
| YAML 键支持 `*`、`**` 通配符，不支持部分匹配（160） | ✅ | `/**`、`/robot1/*`、`/**/planner` 都按预期匹配；`/robot1/plan*` 解析报错 `node name must not contain characters other than alphanumerics or '_'` 🔍 |

### 多个来源设了同一个参数时谁生效 🔍

设计文档没写这条规则，实测结果是：
- **同一个 YAML 里**，键按**在文件里第一次出现的顺序**依次套用，后面的覆盖前面的。和写得是否更具体无关：先写 `/robot1/planner: order: second`、后写 `/**: order: first`，planner 最后拿到 `first`。
- **`-p` 被当成写在 `/**` 下面**。所以对只匹配 `/**` 的节点，`-p` 和 `--params-file` 谁写在命令行后面谁赢。但只要 YAML 里有节点全名的键，并且它出现在 `/**` 之后，`-p` 放在哪里都盖不过它：planner 仍是 `second`。

💡 实用的做法：一个参数只在一个地方设置。确实要分层时，YAML 里按"通配 → 具体"的顺序写。

## 4. 对写代码的影响（规划控制场景）

1. **能用描述表达的约束就写进描述**（`floating_point_range`、`integer_range`、`read_only`、`description`）。这类约束框架会自动检查，`ros2 param describe` 和 rqt 也能看到。跨参数约束才写进校验回调。📐 🔍
2. **一组相关参数一起改时，用原子接口。** 命令行写 `ros2 param set /robot1/planner max_accel 0.2 max_speed 0.3 --atomic`，代码里调 `set_parameters_atomically`。不加 `--atomic` 时是逐个设置：实测 `max_speed 0.3 max_accel 0.5` 的结果是加速度改了，速度被拒绝，只改了一半 🔍。`ros2 param load` 读 YAML 时也是逐个设置，每个参数各自返回结果 🔍（`ros2param` 源码）。
3. **校验回调里只做判断，不要做耗时的事**，也不要在里面改成员变量（见卡片坑 5）。设计文档要求校验"尽快返回" 📐。
4. **监控参数用 `ParameterEventHandler`**，不要自己订阅 `/parameter_events` 再按 `node` 字段过滤。它就是设计文档说的"订阅某个参数的回调" 🔍。
5. **多机器人系统里别用 `-p 节点名:参数`**，它对命名空间里的节点静默无效 🔍。用 YAML 的全名键或 `/**/planner`。
6. **要"全局配置"（例如所有节点共用的 `robot_radius`）时**，ROS 2 没有参数服务器。可以在 launch 里把同一个值传给每个节点，或者用 YAML 的 `/**`。运行时还要能改的话，就另外写一个节点，由它持有这个值，再通过话题或服务分发。💡

## 5. 与其他概念的关系

- 卡片：[参数](../cards/configuration/parameter.md) · 练习：[参数的规则与陷阱](../labs/parameter-rules.md)
- [启动参数与重映射](../cards/configuration/node-arguments.md)：`-p`、`--params-file` 的写法；命令行参数的设计见 [名字与重映射的设计](remapping-design.md)
- [launch 的设计](launch-design.md)：参数怎么在启动时传进去
- [QoS 的设计](qos-design.md)：QoS overrides 也是参数（只读）
- 练习：[配置与启停的设计还成立吗](../labs/config-design-check.md)（第 2 部分）
- 上一级：[总览脑图](../map/overview.md)

## 6. 自测题（先自己答，再看[答案](../answers/parameter-design-answers.md)）

1. 局部规划器有两个参数 `max_vel_x`（范围 0–1.5）和 `acc_lim_x`，要求 `acc_lim_x <= 2 * max_vel_x`。现在要把速度从 1.0 降到 0.3，加速度从 1.5 降到 0.5。（a）分别用 `ros2 param set` 先改速度、再改加速度，第一步会发生什么？（b）应该怎么改才不会卡在中间状态？（c）范围 0–1.5 和"加速度不超过两倍速度"这两条约束，各应该写在哪里？为什么？
2. 可执行文件 `multi_planner` 在一个进程里创建了 `/robot1/planner` 和 `/robot2/planner`，用 `ros2 run my_pkg multi_planner --ros-args -p planner:use_sim_time:=true` 启动，结果两台机器人都没用上仿真时间，日志里也没有任何警告。（a）解释为什么。（b）给出两种能让两个 planner 都用上仿真时间的写法。（c）ROS 1 的参数服务器下这类问题会怎么表现？对比说明 ROS 2"参数挂在节点上"这个设计的代价和好处。
