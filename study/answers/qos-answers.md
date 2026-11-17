# 答案：QoS

> 先在[卡片](../cards/communication/qos.md)里自己写下答案再来对照。

## 题 1：/scan 的订阅

1. `create_subscription<LaserScan>("scan", 10, cb)` 用的是默认 QoS，Reliability = Reliable。📄 激光驱动 `SensorDataQoS` 是 BestEffort。BestEffort 发布 + Reliable 订阅**不兼容**，收不到任何数据。📄 🔍 实验 `be-pub-rel-sub` 收到 0 条。
2. 把订阅改成 `SensorDataQoS` 后，你的节点能收到。📄
3. 调试工具用 Reliable 订阅，**它收不到**，但**不影响你的节点**。📄 文档原文：一对端点是否兼容，不受其他发布者和订阅者的影响；一个发布者可以同时连接 QoS 不同的多个订阅者。

## 题 2：/cmd_vel 超时刹车

- 底盘（订阅端）：`rclcpp::QoS(10).deadline(200ms)`，并注册 `event_callbacks.deadline_callback`，回调里刹车。
- 控制器（发布端）：也要 `.deadline(d)`，且 `d ≤ 200ms`。📄 Deadline 兼容规则：发布端 x、订阅端 y 时要求 y ≥ x。

**控制器忘了设 Deadline：** 发布端是 default（无限），订阅端是 200 ms，**不兼容**，底盘一条指令都收不到。📄 底盘如果在"收不到"时刹车，表现为机器人根本不动；如果它只在 deadline 事件里刹车，而连接都没建立，可能连 deadline 事件也不触发（💡 推导：deadline 是针对已匹配的发布者计时，未匹配时是否触发取决于实现，请自己用 qos_lab 改一改验证）。

**这种安全设计的风险（💡）：**
- QoS 配置错误会让系统"安静地"不工作，必须同时注册 incompatible 事件回调并报警。
- 安全功能依赖中间件实现细节，正式的安全停车还应该有独立的看门狗（watchdog），例如底盘固件里的指令超时。
