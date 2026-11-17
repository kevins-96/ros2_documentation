# 答案：tracing 性能追踪

> 先在[卡片](../cards/observability/tracing.md)里自己写下答案再来对照。

## 题 1：控制器设定 20 Hz，实际 14 Hz

**开哪些事件和上下文：**

```bash
lttng create ctrl --output=/tmp/traces/ctrl
lttng enable-event -u 'ros2:*'
lttng add-context -u -t vtid -t procname
lttng start
```

`ros2:*` 里需要的是：`callback_start`/`callback_end`（回调时长）、`rclcpp_callback_register` 和各种 `*_init`（把地址对应到"哪个节点的哪个定时器/订阅"）、`rclcpp_executor_*`（执行器每一轮选中了谁）、`rclcpp_publish`/`rmw_take`（消息进出）。`vtid` 用来区分线程，多线程执行器时必需。

**什么时候开会话：** 在启动控制器**之前**。🔍 初始化事件只在构造时出现一次，开晚了只能看到回调地址。

**看哪几个指标：**

| 指标 | "回调本身太慢" | "在执行器里排队" |
|---|---|---|
| 控制定时器回调的时长（`callback_end - callback_start`） | 接近或超过 50 ms | 远小于 50 ms |
| 控制回调两次开始之间的间隔 | 约等于回调时长（🔍 练习：回调 120 ms → 周期 120.3 ms） | 时长短，但间隔忽大忽小 |
| 同一线程上，控制回调开始之前在跑什么 | 没什么，执行器一直在跑它 | 前面紧挨着别的回调（比如处理点云、代价地图更新），控制回调等它们结束才开始 |

第二种情况的解决办法在[回调组](../cards/concurrency/callback-group.md)和[执行器](../cards/concurrency/executor.md)：把控制回调放进独立的互斥回调组，用多线程执行器，或者把耗时处理挪到别的节点或进程。第一种情况才需要优化算法本身。

## 题 2：定时器错过触发后的行为

🔍 回调 120 ms、周期 100 ms 时，实测平均周期 120.3 ms（间隔 0.120–0.121 s），不是 200 ms。说明 rclcpp 定时器在回调结束时如果已经过了下一次触发时间，**会马上再次触发**，而不是跳到下一个 100 ms 整数倍的时刻再触发；同时它也**不会补发**错过的次数（没有出现"连续几次 0 间隔"的追赶）。结果是：回调比周期长时，实际周期 ≈ 回调时长。

所以 `work_ms=250` 时预测约 $1/0.25\,\text{s} = 4\,\text{Hz}$。🔍 实测 `average rate: 3.994`，间隔在 0.208–0.292 s 之间波动（比 120 ms 时的抖动大，原因本次没有追查）。

💡 对控制回路的含义：频率下降是"悄悄的"，不会有任何警告；而且控制器里如果用固定的 $\Delta t = 1/f_{\text{设定}}$ 做积分或预测，实际 $\Delta t$ 变长，控制效果会变差。用 `now() - last_time` 算真实的 $\Delta t$，并监控"回调时长 / 周期"。
