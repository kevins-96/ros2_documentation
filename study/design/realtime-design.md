---
tags: [ros2, 设计理念, 实时, 内存分配, 分配器]
distro: rolling
status: design-batch-10
aliases: [实时系统的设计, 实时设计, Real-time Systems, Realtime Proposal, 自定义分配器]
---

# 实时系统的设计：实时是什么、ROS 2 打算怎么支持

> 适用：ROS 2 **Rolling**（2026-11-23 在 `ros:rolling-ros-base`、rclcpp 33.2.0、rmw_fastrtps_cpp 9.6.1、Fast DDS 3.6.2 上实测）
> 来源标记：📐 设计文档原文 · 📄 本仓库文档原文 · 🔍 源码/实测核实 · 💡 经验补充
> 设计文档（[ros2/design](https://github.com/ros2/design) @ `93a415b`，本地副本在 `external/ros2_design/articles/`）：
> [Introduction to Real-time Systems](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/120_realtime_background.md)（2016-01，2016-05 修订）、
> [Proposal for Implementation of Real-time Systems in ROS 2](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/121_realtime_proposal.md)（2016-01）
> 先读卡片 [实时编程](../cards/concurrency/realtime.md)（`mlockall`、`SCHED_FIFO`、抖动实测）。本篇讲设计文档的思路，以及 ROS 2 自己做到了哪一步。

## 1. 设计要回答的问题

**实时（real-time）不等于低延迟（low latency）** 📐。实时系统保证"在截止期（deadline）之前算完"，关键是调度确定、最坏延迟可测、有上限。很多实时应用也要求低延迟，但两者不是一回事。

按**错过截止期之后怎么办**分三类 📐：

| 类型 | 错过截止期 | 例子 |
|---|---|---|
| 硬实时（hard real-time） | 算作系统失效 | 飞控、行星车 |
| 固实时（firm real-time） | 不算失效，但迟到的结果作废 | 机器人装配线 |
| 软实时（soft real-time） | 不算失效，结果照用，服务质量下降 | 音视频播放 |

121 补充了一点：失效模式和延迟特性是两个独立的维度。不过硬实时系统一般也要求抖动（jitter）很低 📐。

要做到实时，**操作系统和用户代码两边都要确定**。非实时内核上跑确定的代码不行，实时内核上跑不确定的代码也不行 📐。ROS 2 的目标没有那么大，它不负责做出实时系统，只保证**自己的核心库不妨碍**用户写实时代码 📐。

## 2. 考虑过的方案与取舍

### 实时代码的写法（120）📐

实时程序通常分三段：**初始化段**（非实时，在这里分配内存、创建线程）、**实时段**（一般是一个循环）、**收尾段**（非实时，在这里释放资源）。设计文档里的"实时代码路径"指的就是中间那一段。

| 问题 | 设计文档给的办法 | 代价 |
|---|---|---|
| 缺页（page fault）要从磁盘读页，耗时不确定 | `mlockall(MCL_CURRENT\|MCL_FUTURE)` 锁住内存，再 `memset` 一遍栈，把栈预先摸一遍 | 要有足够的 `ulimit -l` 权限（卡片坑 2） |
| `malloc` / `new` 可能缺页，堆碎片让分配耗时不定 | ① `mallopt` 关掉 trim 和 mmap，先 malloc 一大块再 free，堆就留在锁住的内存里；② 换成 O(1) 分配器，例如 TLSF；③ 全局变量、静态数组 | ① 要准确估计内存上限，STL 容器的大小无界，所以有风险；② 多一个依赖，测试少；③ 全局变量的老问题 |
| 继承层次深、PIMPL 让缓存命中差 | 少用 | — |
| 异常压栈多 | 正确使用就不必担心（异常本来就意味着出了错） | — |
| 磁盘 I/O、打印 | 放到循环外面，或者交给非实时线程 | — |
| 优先级反转（priority inversion） | 不用阻塞的同步原语、优先级继承、无锁数据结构等 | — |
| 周期漂移 | 每周期按绝对时间算下一次"出手时刻"，用高精度的 `nanosleep` 睡到那一刻 | — |
| `fork` 写时复制会缺页 | 别在实时进程里 `fork` | — |

"知道你的内存需求"这一条最实用 📐：

| 内存大小什么时候知道 | 例子 | 办法 |
|---|---|---|
| 编译期 | 发布定长消息 | 栈上的定长对象 |
| 运行期，但在进入实时段之前 | 消息大小由命令行参数决定 | 初始化段一次性分配好 |
| 实时段里才算出来 | 传感器数据决定要发多大的消息 | 对象池、TLSF，或者用栈、超出就报错 |

### ROS 2 怎么"实时加固"（120 结尾）📐

| 方案 | 优点 | 缺点 |
|---|---|---|
| 整个栈加一个"实时友好"模式开关 | 能动态切换 | 重构量大，和现有代码融合可能做不下去 |
| 另写一套实时栈（rclrt、rmwrt） | 好设计、好维护，实时代码和现有代码隔离 | 要维护的包更多，用户不方便 |
| **rcl / rmw 一层做到实时安全，再配一个实时安全的语言封装（rclrt 或 rclc）** | **工作量最小、收益最多**：用户可以把分配策略注入 rcl/rmw，同步放在最上面的语言层 | 要测实时安全的代码更多，灵活性带来复杂度 |

设计文档选了第三种。

### 121 的具体计划 📐

- **迭代、测试驱动**：选两个极端用例实现、测量、找瓶颈、改 API，然后重复。一个是 STM32 无操作系统、1 kHz、抖动 3%；另一个是 x86 上的 RT_PREEMPT 或 Xenomai，同时还跑非实时的人机交互进程。
- **内存**：rclcpp 的发布者、订阅者接受自定义分配器（allocator），并传递到内部用的标准库容器。`realtime_support` 包提供 TLSF 分配器和示例。还要做的事包括：rclcpp 只要求用户给"分配/释放"两个函数加一个状态指针；服务、客户端、参数也支持分配器；rcl、rclc、rmw 全部实现并测试分配器接口。
- **同步**：能用原子量（atomic）就别用互斥锁，但很多原子类型并不是无锁的，要用 `std::atomic_is_lock_free` 检查；同步原语藏在一层抽象后面，可以注入 RTOS 的原语。当时 Executor 和 IntraProcessManager 只是"留了抽象的路"，并没有实时安全的实现。
- **中间件与 QoS**：中间件不实时，整个系统就不实时。DEADLINE 这类 QoS 是为实时设计的，可以考虑在 rmw 里提供一个"实时"QoS 预设。
- **生命周期**：初始化阶段放宽实时约束，在这里预分配、改线程优先级；进入执行阶段后可以自检约束。
- **验证**：每一项性能改进都要有自动测试。例如分配器有一个测试：实时阶段只要调用了系统默认分配器，测试就失败。

## 3. 设计与 Rolling 现状对照

状态：✅ 按设计实现 · 🔄 实现与设计不同 · ⏳ 未实现 · 🗑️ 已过时。🔍 结果来自[练习](../labs/perf-design-check.md)第 1 部分（`rt_alloc_probe`）和头文件。

| 设计文档的说法 | 状态 | Rolling 实测 |
|---|---|---|
| rcl 接受用户提供的分配器（分配、释放函数 + 状态指针） | ✅ | `rcutils_allocator_t` 有 `allocate`、`deallocate`、`reallocate`、`zero_allocate` 四个函数指针，外加 `void * state` 🔍 |
| rclcpp 的发布者、订阅者可以传自定义分配器 | ✅ | `PublisherOptionsWithAllocator<Alloc>`、`SubscriptionOptionsWithAllocator<Alloc>`，订阅端再配一个 `MessageMemoryStrategy<Msg, Alloc>` 🔍 |
| 自定义分配器一路传到 rmw，实时路径上不再调用系统分配器 | ⏳ | 每条 `Int32`：默认分配器下 publish 1 次 + spin 3 次系统 `new`；换成自定义分配器后，只有消息本身那 1 次挪到了自定义分配器，**剩下的 3 次还是系统 `new`** 🔍。调用栈显示它们来自 Fast DDS 的 `DataReaderHistory::add_to_instance` 和 rmw 的 take 路径（`rcl_take` 之下）🔍 |
| 进程内通信路径也走自定义分配器 | ⏳ | 开进程内通信、消息用自定义分配器分配时：publish 时自定义 1 次、系统 2 次（`IntraProcessManager::do_intra_process_publish` 里），spin 时系统 1 次（`SubscriptionIntraProcess` 里）🔍 |
| 服务、客户端、参数也支持自定义分配器 | ⏳ | `create_service`、`create_client` 的模板参数只有服务类型和回调类型，没有分配器 🔍 |
| `realtime_support` 提供 TLSF 分配器和测量工具 `rttest` | ✅ 仍在维护 | rosdistro 的 Rolling 里有 `realtime_support` 1.0.2（包含 `rttest`、`tlsf_cpp`）🔍；`ros-base` 镜像没装 🔍 |
| 实时安全的语言封装 rclc | ✅ | rosdistro 的 Rolling 里有 `rclc` 🔍。💡 micro-ROS 就是用 rclc 跑在微控制器上的 |
| 嵌入式端用 freertps | 🗑️ | rosdistro 里已经没有 freertps 🔍。💡 微控制器现在走 micro-ROS（DDS-XRCE 客户端 + 代理） |
| 实时安全的执行器和 IntraProcessManager（同步原语可替换） | ⏳ | 执行器有 `SingleThreadedExecutor`、`MultiThreadedExecutor`、`EventsCBGExecutor`，以及 `experimental::executors::EventsExecutor`；**`StaticSingleThreadedExecutor` 的头文件已经删了** 🔍。上面统计到 spin 路径还有系统分配，所以执行器路径算不上实时安全 💡 |
| rmw 提供一个"实时"QoS 预设 | ⏳ | `rmw/qos_profiles.h` 里只有 default、sensor_data、services_default、parameters、parameter_events、rosout_default、system_default、best_available、unknown 🔍 |
| DEADLINE 等为实时设计的 QoS | ✅ | 见 [QoS 的设计](qos-design.md) |
| 用生命周期分开"初始化"和"执行"两个阶段 | ✅ | 见 [生命周期的设计](lifecycle-design.md)；在 `on_configure` 里预分配，`on_activate` 之后才进实时段 💡 |
| 原子量不一定无锁，要检查 | ✅ 确实如此 | x86-64 上 `std::atomic<double>`、`std::atomic<int64_t>` 是无锁的；三个 `double` 组成的 24 字节位姿 `std::atomic<Pose3d>` **不是**无锁的 🔍 |

## 4. 对写代码的影响（规划控制场景）

1. **控制环按三段式写，并且对应到生命周期。** 在 `on_configure` 里分配好轨迹缓冲、矩阵和消息，在 `on_activate` 里启动循环，循环里不再 `new`、不再 `resize`。💡
2. **自定义分配器只能挪走"消息本身"那次分配。** 在 Rolling + Fast DDS 上，每条消息还有约 3 次系统分配发生在 rmw 和 DDS 内部 🔍，用户代码改不了。💡 所以硬实时的控制环别直接放在 ROS 回调里。常见做法是：控制环跑在自己的 `SCHED_FIFO` 线程里，和 ROS 回调线程之间用预分配好的无锁缓冲交换数据（ros2_control 的 `realtime_tools` 就是这种做法，rosdistro 的 Rolling 里有）。
3. **线程之间共享位姿，别以为 `std::atomic<Pose>` 就够了。** 24 字节的结构体在 x86-64 上不是无锁的（`is_lock_free()` 返回 0）🔍。💡 GCC 这时会调用 libatomic，用一张全局锁表来实现，实时线程读它时可能被写线程阻塞。可以用一个 `std::atomic<int>` 下标在两块预分配的缓冲之间切换（双缓冲），或者用 seqlock。
4. **学设计文档的验证办法，在单元测试里数分配次数。** 替换全局 `operator new`，在实时段开始前清零，结束后断言为 0。本练习的 `rt_alloc_probe --trace` 还能打印每次分配的调用栈，告诉你分配发生在哪一层 🔍。
5. **别再用 `StaticSingleThreadedExecutor`。** Rolling 已经删了这个头文件 🔍；从 Humble、Jazzy 迁移过来的代码要改成 `SingleThreadedExecutor` 或 `EventsCBGExecutor`（见卡片 [执行器](../cards/concurrency/executor.md)）。
6. **"实时"先定义清楚再谈实现。** 问自己三件事：截止期是多少？错过一次会怎样（硬、固、软）？最坏延迟怎么测？卡片的 `rt_lab` 和 `cyclictest` 测的是最坏延迟，不是平均值。💡

## 5. 与其他概念的关系

- 卡片：[实时编程](../cards/concurrency/realtime.md) · [执行器](../cards/concurrency/executor.md) · [进程内通信与零拷贝](../cards/concurrency/intra-process.md)
- 练习：[实时循环的抖动](../labs/realtime-jitter.md) · [并发与性能的设计还成立吗](../labs/perf-design-check.md)（第 1 部分）
- [进程内通信与零拷贝的设计](intra-process-design.md)：IntraProcessManager 内部的分配
- [rmw 接口与序列化的设计](rmw-serialization-design.md)：rmw 这一层为什么改不了分配
- [生命周期的设计](lifecycle-design.md)：初始化和执行两个阶段
- [QoS 的设计](qos-design.md)：DEADLINE
- 上一级：[总览脑图](../map/overview.md)

## 6. 自测题（先自己答，再看[答案](../answers/realtime-design-answers.md)）

1. 你的 MPC 控制器 100 Hz 运行，每周期要算 6–9 ms。（a）按设计文档的分类，"偶尔一个周期超过 10 ms 就用上一周期的指令"属于硬、固、软哪一类？（b）求解器每周期内部会 `std::vector::resize` 到一个由预测时域决定的长度，预测时域是参数、启动后不变。按"知道你的内存需求"的表格，应该怎么改？（c）同事说"给发布者和订阅者都配上 TLSF 分配器，ROS 通信就不分配内存了"。根据本篇的实测，这句话哪里不对？
2. 控制线程（`SCHED_FIFO`）要读定位线程写的最新位姿 `{x, y, yaw}`。（a）同事写了 `std::atomic<Pose3d> latest_pose;`，在 x86-64 上它实际是怎么实现的？对实时线程有什么风险？（b）设计文档列了几种避免优先级反转的办法，选一种给出你的方案（说清楚写线程和读线程各做什么）。
