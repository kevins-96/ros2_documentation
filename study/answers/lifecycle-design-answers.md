# 答案：生命周期的设计

> 对应：[lifecycle-design.md](../design/lifecycle-design.md)

## 第 1 题

**（a）** 返回 **FAILURE**。在 Rolling 上，`on_activate` 返回 FAILURE 时节点回到 **Inactive** 🔍，串口这类在 configure 里拿到的资源都还在。管理者过 2 s 再请求一次 activate 就行。要注意一点：在 `on_activate` 里已经做了一半的事（比如发过的命令）要在返回 FAILURE 之前自己撤销，因为不会有 `on_deactivate` 来帮你收尾 💡。设计文档原本规定 Activating 中"成功以外的任何结果"都进 ErrorProcessing 📐，按那个设计就做不到"退回 Inactive 稍后重试"。

**（b）** 返回 **ERROR**，或者直接抛异常，效果一样 🔍。节点进入 ErrorProcessing，`on_error` 被调用，收到的来源状态是 `inactive` 🔍。`on_error` 要把串口关掉、把内部状态清空：
- 清理成功就返回 SUCCESS，节点回到 **Unconfigured**。管理者可以重新 configure，重新打开串口、重新初始化雷达 🔍。
- 清理不了（例如串口句柄已经失效、关不掉）就返回 FAILURE，节点进入 **Finalized** 🔍。这时只能销毁节点，通常是重启进程。设计文档说 Finalized 状态是为了让失败的节点还能被内省和调试 📐。

## 第 2 题

**（a）** 界面上显示"未知"或空白，直到某个驱动再发生一次状态变化。原因是 `transition_event` 的 durability 是 **volatile**，不是锁存的 🔍：变化发生时监控节点还没订阅，这些消息就错过了。设计文档本来规定状态话题**必须锁存**，就是为了让后加入的订阅者也能拿到当前状态 📐。

**（b）** 监控节点启动时，先对每个驱动调一次 `~/get_state` 服务拿到当前状态，再靠 `transition_event` 跟踪后续变化 💡。两者之间如果有变化，可能会漏掉一次。可以在每条 `transition_event` 到来时核对一下它的 `start_state` 是否和界面上记录的一致，不一致就再调一次 `get_state`。💡 也有人在订阅端用 transient_local，但发布端是 volatile 时这样做没用，因为 durability 要发布端也提供才行（见 [QoS](../cards/communication/qos.md)）💡。

**（c）**
- 订阅回调**会被调用**：Rolling 只管理 `LifecyclePublisher`，订阅不受状态控制 🔍。
- 普通 `rclcpp::Publisher` 发布的 `/cmd_vel` **不会被挡住**，因为它不是受管理的实体，Inactive 时照样发出去 💡（推导：只有 `LifecyclePublisher` 会在 publish 时检查是否已激活）。

设计文档的说法是 Inactive 时"受管理的话题不读、不处理数据" 📐，Rolling 只实现了发布这一侧。所以控制器的 `/cmd_vel` 一定要用 `create_publisher`，它在 `LifecycleNode` 上返回的是 `LifecyclePublisher`，还要记得在 `on_activate` 里调用基类，否则发布者一直处于未激活状态（卡片坑 1）。订阅回调里再检查一下状态，避免在 Inactive 时浪费算力。💡
