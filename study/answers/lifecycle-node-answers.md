# 答案：生命周期节点

> 先在[卡片](../cards/configuration/lifecycle-node.md)里自己写下答案再来对照。

## 题 1：底盘驱动的操作放在哪个回调

| 操作 | 回调 | 理由 |
|---|---|---|
| 声明 `port` 参数 | 构造函数 | 💡 参数要在节点一出现时就能被 `ros2 param` 看到和修改，configure 时再读取 |
| 打开串口 | `on_configure` | 💡 资源分配属于 configure；失败时节点留在 unconfigured，改了参数可以再试 |
| 创建 `/odom` 发布者 | `on_configure` | 💡 创建对象但还不发布；用 `LifecyclePublisher` |
| 开始发布 | `on_activate` | 🔍 必须调用 `LifecycleNode::on_activate(state)`，否则 `LifecyclePublisher` 没被激活，消息会被丢弃 |
| 急停时发零速度并停止发布 | `on_deactivate` | 💡 先发零速度，再调用基类 `on_deactivate`。顺序反了，零速度会被未激活的发布者丢掉 |
| 关闭串口 | `on_cleanup`（以及 `on_shutdown`、`on_error`） | 💡 cleanup 要撤销 configure 做的一切；shutdown 和 error 可能从任何状态进入，也要能安全关闭 |

**串口打不开返回 FAILURE 还是 ERROR？**

💡 一般返回 **FAILURE**。🔍 实测 FAILURE 让节点留在 `unconfigured`，什么都没坏，外部可以改 `port` 参数后重新 configure。

ERROR 表示"出现了回调自己处理不了的异常状态"。🔍 实测 ERROR 会进入 `on_error`；`on_error` 返回 SUCCESS 时回到 `unconfigured`，否则进入 `finalized`，节点就再也不能用了，只能重启进程。打不开串口是可以预期、可以重试的失败，不需要走错误处理。

💡 注意：FAILURE 之前要把这次 configure 里已经创建的东西释放掉（比如已经创建的发布者），否则下一次 configure 会重复创建。

## 题 2：显示 active 但 `/cmd_vel` 上没有消息

最可能的原因：同事重写了 `on_activate`，但**没有调用基类的 `LifecycleNode::on_activate(state)`**。

🔍 实测现象完全吻合：
- `ros2 lifecycle get` 显示 `active [3]`，因为回调返回了 SUCCESS，状态机照常转换。
- `LifecyclePublisher::is_activated()` 是 `false`，`publish()` 的消息都被丢弃。
- 日志里只有**一条** warning：`Trying to publish message on the topic '...', but the publisher is not activated`。每次进入未激活状态只警告一次，所以很容易被忽略。

改法：

```cpp
CallbackReturn on_activate(const rclcpp_lifecycle::State & state) override
{
  LifecycleNode::on_activate(state);   // 激活所有 LifecyclePublisher
  // ... 自己的激活逻辑
  return CallbackReturn::SUCCESS;
}
```

`on_deactivate` 同样要调用 `LifecycleNode::on_deactivate(state)`。💡 也可以对单个发布者调用 `pub_->on_activate()`，但调用基类更不容易漏掉新加的发布者。
