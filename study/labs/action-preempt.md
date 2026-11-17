# 练习：动作服务端忙的时候来了新目标

> 适用：ROS 2 **Rolling**。对应卡片：[动作](../cards/communication/action.md)
> 代码：[comm_lab/src/action_lab.cpp](comm_lab/src/action_lab.cpp)，接口 [MoveTo.action](study_interfaces/action/MoveTo.action)
> 用时：约 20 分钟

## 场景

导航动作正在执行目标 A（约 2 秒），用户在 t≈1.5 s 时又点了目标 B。服务端可以：

- `--policy reject`：忙时拒绝新目标
- `--policy preempt`：接受新目标，中止（abort）旧目标

编译方法同 [QoS 练习](qos-mismatch.md)。

```bash
ros2 run comm_lab action_lab --policy reject
ros2 run comm_lab action_lab --policy preempt
```

## 先填预测

| `--policy` | A 的结果码 | B 是否被接受 | B 的结果码 |
|---|---|---|---|
| reject | | | |
| preempt | | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

2026-11-17 在 `ros:rolling-ros-base` 里实测：

```text
# reject
[A] send goal x=1.0
[A] accepted
[B] send goal x=2.0
busy -> REJECT goal x=2.0
[B] REJECTED
[A] result SUCCEEDED

# preempt
[A] send goal x=1.0
[A] accepted
[B] send goal x=2.0
new goal x=2.0 preempts the active goal
[B] accepted
[A] result ABORTED        # 约 1 ms 后，A 的执行线程发现自己已不是活动目标
[B] result SUCCEEDED
```

注意：
- 被抢占的 A 结果码是 **ABORTED**（服务端发起），不是 CANCELED（客户端发起）。客户端如果把所有 ABORTED 都当失败处理，就会在用户换目标时误触发恢复行为。
- 抢占是在**服务端代码里自己实现**的（`active_` 指针 + 执行循环里检查），rclcpp_action 不会替你做。

</details>

## 挑战（选做）

1. 实现第三种策略 `queue`：忙时接受新目标，等 A 完成后再执行 B。想一想：B 被接受后、开始执行前，客户端收到的反馈应该是什么？
2. 在客户端加一个"t≈1.0 s 取消 A"的步骤（`async_cancel_goal`），观察 A 的结果码变成什么。
3. 现在 `execute()` 每个目标开一个线程，并保存在 `workers_` 里直到析构。如果一小时里来了 1 万个目标会怎样？怎么改？
