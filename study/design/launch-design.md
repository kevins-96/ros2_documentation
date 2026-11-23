---
tags: [ros2, 设计理念, launch, 组件容器]
distro: rolling
status: design-batch-9
aliases: [launch 的设计, launch 设计理念, ROS 2 Launch System design, launch XML 设计, 容器进程接口]
---

# launch 的设计：进程和节点分开、事件驱动、前端只是外壳

> 适用：ROS 2 **Rolling**（2026-11-23 在 `ros:rolling-ros-base`、launch 3.10.3、launch_ros 0.30.3、rclcpp_components 33.2.0 上实测）
> 来源标记：📐 设计文档原文 · 📄 本仓库文档原文 · 🔍 源码/实测核实 · 💡 经验补充
> 设计文档（[ros2/design](https://github.com/ros2/design) @ `93a415b`，本地副本在 `external/ros2_design/articles/`）：
> [ROS 2 Launch System](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/150_roslaunch.md)（2019-09）、
> [ROS 2 Launch XML Format](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/151_roslaunch_xml.md)（2019-09，2021-06 修订）、
> [ROS 2 Launch Static Descriptions - Implementation Considerations](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/152_roslaunch_frontend.md)（2019-09，2020-07 修订）
> 先读卡片 [launch 启动系统](../cards/configuration/launch.md)（动作、替换、事件处理器的写法）和 [组件与组合](../cards/concurrency/component.md)。

## 1. 设计要回答的问题

ROS 1 的 `roslaunch` 在 ROS 2 里遇到这些新情况 📐：

| ROS 1 的做法 | ROS 2 的新情况 |
|---|---|
| 一个进程只有一个节点，"节点"和"进程"几乎可以互换（nodelet 也靠代理进程保持这种对应） | 一个进程可以有多个节点，关掉一个节点不再等于给某个进程发信号 |
| 远程启动靠 SSH | Windows 也是目标平台，当时没有原生 SSH |
| launch 和全局参数服务器紧密耦合 | 参数挂在节点上，没有全局参数（见 [参数的设计](parameter-design.md)） |
| 对进程退出只有两种反应：`respawn`、`required` | 希望能拿到更细的退出信息，并让用户自定义反应 |
| "不保证节点的启动顺序，因为根本没法知道节点什么时候初始化完" | 生命周期节点有明确的状态，可以说"等相机驱动进入 Active 后再启动图像处理" |
| 大家只用 XML，动态逻辑靠 xacro 或 `$(eval)` 硬塞 | 希望有一个好用、有文档的编程 API，静态描述只是它的一种写法 |

## 2. 考虑过的方案与取舍

### 三种被启动的东西，逐层继承 📐

- **操作系统进程**：执行（命令、工作目录、环境变量）；运行时（stdin/stdout/stderr、信号）；终止（退出码）。
- **ROS 节点**：在进程的基础上加 ROS 专有的配置，即重映射、参数和命名空间，通过命令行传进去。
- **生命周期节点（managed node）**：再加状态变化事件，launch 可以据此编排。

### 终止：信号升级 📐

launch 要停掉一个进程时，先发 `SIGINT`，过一段时间发 `SIGTERM`，再过一段时间发 `SIGKILL`。每个进程可以调整这两个时间，也可以跳过后两步。**如果 launch 自己收到 `SIGTERM`**，说明用户已经等不及了，设计是立刻对所有子进程发 `SIGKILL` 然后退出，免得用户去 `SIGKILL` launch 本身、留下僵尸进程。

### 容器进程的 API：三种方案比出来的 📐

动态加载节点时，launch 要告诉容器"加载哪个节点、带什么参数"。设计文档比较了三种方式：

| 方案 | 优点 | 缺点 |
|---|---|---|
| 命令行传配置文件 | 不用等发现，延迟低 | 需要可写的文件系统；不知道加载成功没有；不能卸载 |
| 通过 STDIN 传 | 只读文件系统也能用，可以卸载 | 不知道成功没有；没法阻止被加载的节点读 STDIN |
| **ROS 服务** | 能返回成功或失败，可以内省，能确认容器支持这个接口 | 要等服务可用，延迟最高 |

最后选了 ROS 服务：`~/_container/load_node`、`unload_node`、`list_nodes`，都是隐藏服务，避免和用户的服务撞名。附带几条规则 📐：
- 容器自己的命令行参数（重映射、参数、日志级别）**不能作用到动态加载的节点**上。
- 加载的节点全名和已有节点相同时**必须拒绝**，否则参数这类依赖"节点名唯一"的功能会冲突。
- 每个加载的节点分配一个唯一的 id，而且 id 要隔很久才能复用。

### 事件系统 📐

事件分三类：launch 自身的事件（例如执行到某个动作）、操作系统的事件（进程启动、退出、输出）、ROS 的事件（生命周期状态变化）。用户注册事件处理器，可以按条件过滤，再触发任意动作。`respawn` 和 `required` 只是事件处理器的特例。

### 前端（XML、YAML）怎么接到 Python 实现上（152）📐

| 方案 | 做法 | 取舍 |
|---|---|---|
| 正向映射（FDM） | 标签一一对应到 Python 类的构造参数 | 最简单，但要准确的类型信息，标记语言写起来也不自然 |
| 正向映射 + 标记辅助（FDM+） | 在上面的基础上，每种标记语言可以加自己的语法糖 | 多种写法表示同一个概念，容易混乱 |
| **抽象描述解析（ADP）** | 先把 XML 或 YAML 解析成与语言无关的"实体树"，每个动作类自己实现"从实体树构造自己" | 前端和后端解耦，新标记语言只要实现实体树；代价是 launch 的实现要知道解析过程 |

## 3. 设计与 Rolling 现状对照

状态：✅ 按设计实现 · 🔄 实现与设计不同 · ⏳ 未实现 · 🗑️ 已过时。🔍 结果来自[练习](../labs/config-design-check.md)第 5 部分（`signal_escalation.launch.py`、`lifecycle_chain.launch.py`、`frontend_demo.launch.*`、`name_probe_component`）。

| 设计文档的说法 | 状态 | Rolling 实测 / 源码 |
|---|---|---|
| 停进程时 SIGINT → SIGTERM → SIGKILL，时间可调 | ✅ | 子进程忽略 SIGINT 和 SIGTERM：对 launch 发 SIGINT 后，子进程 5.0 s 后收到 SIGTERM，再 5 s 后被 SIGKILL（退出码 -9）🔍。默认值来自 `sigterm_timeout`、`sigkill_timeout` 这两个 **launch 配置变量**，默认各 5 s（`launch/actions/execute_local.py`）🔍。因为是配置变量，即使 launch 文件没有声明，命令行写 `sigterm_timeout:=1 sigkill_timeout:=1` 也会改掉**所有**进程的升级时间（实测 SIGTERM 提前到 1 s）🔍。日志里的 "failed to terminate '10.0' seconds after receiving 'SIGTERM'" 其实是两段时间的**累计**，SIGTERM 之后只等了 5 s 🔍 |
| launch 收到 SIGTERM 时，立刻对子进程发 SIGKILL | 🔄 | 和收到 SIGINT 的处理**完全一样**：先给子进程发 SIGINT，同样等 5 s + 5 s 才 SIGKILL，总共约 10 s 🔍 |
| 远程机器上启动进程 | ⏳ | 没有 💡 |
| 节点和进程分开看待 | ✅ | `Node` 动作对应一个进程，`ComposableNode` 对应容器里的一个节点 |
| launch 把 `name=` 等翻译成命令行参数 | ✅，但有坑 | `Node(name='consumer')` 变成 `-r __node:=consumer`，作用于这个进程里的**所有**节点：一个进程两个节点时，两个都叫 `consumer` 🔍 |
| 用生命周期事件编排启动顺序 | ✅ | `OnStateTransition(goal_state='inactive')` 里请求 activate，`goal_state='active'` 里再启动下游节点，实测顺序为：on_configure → "已 inactive" → on_activate → "已 active，启动 consumer" → consumer 启动 🔍 |
| 生命周期节点可以提供"自动 configure 加 activate"的选项（node_lifecycle） | ✅ | `launch_ros` 的 `LifecycleNode(autostart=True)`，默认 `False` 🔍（源码） |
| 容器服务 `~/_container/load_node` 等，隐藏 | ✅ | `/cont/_container/load_node`、`unload_node`、`list_nodes`。`ros2 service list` 默认看不到，加 `--include-hidden-services` 才显示 🔍 |
| 容器的命令行参数不作用到加载的节点 | ✅ | 容器带 `-r __node:=cont -r scan:=container_scan -p tag:=from_container`，加载的节点仍叫 `/name_probe`，`scan` 仍是 `/scan`，`tag` 也没设上；加载时自己带的 `-r scan:=own_scan -p tag:=own` 生效 🔍 |
| 全名重复的节点**必须拒绝** | 🔄 | 同一个组件加载两次都成功，得到两个 `/name_probe`（id 1、2），只有 rosout 的同名警告 🔍 |
| 每个加载的节点有唯一 id | ✅ | 1、2、3 递增 🔍 |
| XML 前端：`<let>`、`<arg>`、`if`/`unless`、`$(var)`、`$(eval)`、`<param value-sep>` | ✅ | `frontend_demo.launch.xml` 和等价的 `.py` 得到完全相同的参数（`frame_ids = [map, odom, base_link]`、`limits.accel = 1.0`）和重映射；`use_lidar:=false` 时节点不启动 🔍 |
| 前端采用抽象描述解析（ADP） | ✅ | `launch/frontend/entity.py` 里的 `Entity` 抽象出 XML/YAML 的差别，各个动作类用 `@expose_action('node')` 注册，并实现 `parse(entity, parser)` 类方法 🔍（源码） |
| — | 🔍 新发现 | XML 注释里不能出现连续的两个减号，所以在注释里写 `--show-args` 这类命令行选项，整个文件都会解析失败（`not well-formed (invalid token)`）。报错信息只给行列号，不提注释 |
| 用户自定义替换（XML 里） | ⏳ | 设计文档写的是 TBD；Python 里可以自己写 `Substitution` 子类 💡 |
| 模拟 ROS 1 的全局参数服务器节点，可随 launch 自动启动 | ⏳ | 没有（见 [参数的设计](parameter-design.md)） |

## 4. 对写代码的影响（规划控制场景）

1. **驱动和控制器要在 SIGINT 后几秒内退出。** launch 等 5 s 才升级到 SIGTERM，再等 5 s 才 SIGKILL 🔍。如果收到 SIGINT 后要做"让电机停下"这类收尾，要在 5 s 内做完。确实要更长时间时，调大这个进程的 `sigterm_timeout`。💡
2. **别指望 `kill <launch 的 pid>` 能快速清场。** 发 SIGTERM 和发 Ctrl-C 一样，要等升级流程走完 🔍。脚本里要强制停止时，对 launch 的进程组发信号。💡
3. **多节点的可执行文件不要在 launch 里给 `name=`**，否则所有节点同名 🔍。要么给每个节点写带前缀的重映射规则，要么改成组件放进容器，每个节点单独命名。
4. **用生命周期事件保证启动顺序**，不要靠 `TimerAction` 延时几秒。延时只是"大概率够用"，而 `OnStateTransition` 等的是对方真的进入了 Active 📐 🔍。
5. **组件容器不会拦住同名节点** 🔍。在 launch 里给每个 `ComposableNode` 显式写不同的 `name`，尤其是同一个插件加载多份时（例如两个代价地图层）。
6. **XML 和 Python 二选一时**：结构固定、给别人改参数的用 XML，可读性好；有循环、要算东西的用 Python。两者最后得到的是同一套动作对象 📐 🔍。XML 注释里别写 `--`。

## 5. 与其他概念的关系

- 卡片：[launch 启动系统](../cards/configuration/launch.md) · 练习：[launch 与名字解析](../labs/launch-and-names.md)
- [组件与组合](../cards/concurrency/component.md)：容器进程 API 的另一面
- [生命周期的设计](lifecycle-design.md)：launch 编排依赖的状态机
- [名字与重映射的设计](remapping-design.md)、[参数的设计](parameter-design.md)：launch 翻译成命令行参数的那一层
- 练习：[配置与启停的设计还成立吗](../labs/config-design-check.md)（第 5 部分）
- 上一级：[总览脑图](../map/overview.md)

## 6. 自测题（先自己答，再看[答案](../answers/launch-design-answers.md)）

1. 底盘驱动的 SIGINT 处理函数里会先发零速度，再等电机控制器确认，最长要 8 s。用默认设置的 launch 启动它。（a）用户按 Ctrl-C 后会发生什么？电机可能处于什么状态？（b）运维脚本改用 `kill $(pgrep -f "ros2 launch")` 停机器人，按设计文档会发生什么？在 Rolling 上实际会发生什么？（c）给出两种改法。
2. 感知流水线要求"相机驱动 Active 之后才启动检测节点"。（a）为什么 ROS 1 的 `roslaunch` 做不到这一点，ROS 2 靠什么做到？（b）有人用 `TimerAction(period=3.0, ...)` 延时启动检测节点，这种做法在什么情况下会出问题？（c）检测节点用组件放进容器，同一个检测组件要加载两份分别处理左右相机，launch 里漏写了 `name`。会发生什么？设计文档对这种情况的要求和 Rolling 的实际行为有什么不同？
