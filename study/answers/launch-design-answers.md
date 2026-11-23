# 答案：launch 的设计

> 对应：[launch-design.md](../design/launch-design.md)

## 第 1 题

**（a）** launch 先给驱动发 SIGINT，驱动开始收尾。5 s 后 launch 认为它"没退出"，发 SIGTERM；再过 5 s 发 SIGKILL 🔍。收尾要 8 s，正好跨过了第 5 s 的 SIGTERM。

- rclcpp 默认对 SIGINT 和 SIGTERM 都安装了处理函数，作用都是让 context 关闭 💡（推导：rclcpp 的信号处理器同时处理这两个信号）。所以第 5 s 的 SIGTERM 会不会打断收尾，要看收尾代码怎么写：如果它依赖已经关闭的 context，比如还在用 executor 等确认消息，就可能提前失败。
- 第 10 s 的 SIGKILL 不能被捕获。8 s 的收尾只要不被 SIGTERM 打断，能在 SIGKILL 之前做完，但余量只有 2 s。

如果收尾被打断，电机可能停在"零速度指令已发出、但还没确认"的状态，也可能根本没收到零速度。💡

**（b）** `kill` 默认发 SIGTERM。

- 按设计文档：launch 收到 SIGTERM 后立刻对所有子进程发 SIGKILL 然后退出 📐。驱动连收尾的机会都没有，这是最坏的情况。
- 在 Rolling 上：SIGTERM 和 SIGINT 的处理完全一样，先给子进程发 SIGINT，再按 5 s + 5 s 升级 🔍。所以结果和 (a) 一样。

**（c）** 两种改法：
- 在这个进程上调大升级时间：`ExecuteProcess`/`Node` 的 `sigterm_timeout='12'`，或者在命令行上设 launch 配置 `sigterm_timeout:=12`（它是全局的 launch 配置变量，会影响所有进程）🔍。
- 把"安全停车"从信号处理中拿出来，交给生命周期管理：停机时先对驱动做 deactivate 和 shutdown 转换，由 `on_deactivate` 发零速度并等确认，确认后才结束 launch。这样收尾有明确的完成信号，不靠超时。💡 另外，电机控制器自己也应该有"指令超时就停"的保护，不能只靠上位机正常退出。💡

## 第 2 题

**（a）** ROS 1 的 roslaunch 文档原话是：没有办法知道一个节点什么时候初始化完，所以启动顺序没有意义 📐。ROS 2 的生命周期节点有明确的状态，状态变化会通过 `transition_event` 话题发出来。launch 用 `OnStateTransition` 事件处理器监听这些变化，等相机进入 `active` 再启动检测节点 📐 🔍。

**（b）** 延时只是"大概率够用"💡：
- 相机初始化慢时，3 s 不够，例如 USB 重新枚举、驱动加载标定文件、机器负载高。检测节点启动时相机还没出图，只能自己再处理"没数据"的情况。
- 相机 configure 失败时，延时启动照样会把检测节点拉起来，系统看上去是起来了，其实是坏的。
- 反过来，相机很快就绪时，白白多等 3 s。

`OnStateTransition` 等的是"真的进入 Active"。如果相机一直到不了 Active，下游也不会启动，问题更容易发现 🔍。

**（c）** 两份组件都会用代码里的默认节点名，比如 `/detector`，得到两个同名节点 🔍。
- 设计文档的要求：全名相同的节点**必须拒绝加载**，因为参数这类功能依赖"节点名唯一" 📐。
- Rolling 的实际行为：两份都加载成功，只有 rosout 打印一条同名警告 🔍。

后果是两个节点的参数服务、私有话题同名，`ros2 param set /detector ...` 改的可能不是你想要的那一个。💡 所以要给每个 `ComposableNode` 显式写 `name`，比如 `left_detector`、`right_detector`。
