---
tags: [ros2, rclcpp, 通信, 话题, 进阶]
distro: rolling
status: batch-1
aliases: [话题进阶, 内容过滤订阅, Content Filtering, 键控话题, Topic Keys, wait_for_all_acked]
---

# 话题进阶：内容过滤、键控话题、等待确认

> 适用：ROS 2 **Rolling**，rclcpp。三个功能都**依赖 RMW 实现支持**，换中间件前先查下表。
> 来源标记：📄 仓库文档原文 · 🔍 rclcpp 源码/实测核实 · 💡 经验补充（可信，但请自己验证）
> 源文档：[Content-Filtering-Subscription.rst](../../../source/ROS-Framework/interfaces/topics/Working-with-topics/Content-Filtering-Subscription.rst)、[Topic-Keys-Tutorial.rst](../../../source/ROS-Framework/interfaces/topics/Working-with-topics/Topic-Keys/Topic-Keys-Tutorial.rst)、[Filtered-Topic-Keys-Tutorial.rst](../../../source/ROS-Framework/interfaces/topics/Working-with-topics/Topic-Keys/Filtered-Topic-Keys-Tutorial.rst)、[Wait-for-Acknowledgment.rst](../../../source/ROS-Framework/interfaces/topics/Working-with-topics/Wait-for-Acknowledgment.rst)

## 1. 一句话定义

三个让话题"更聪明"的中间件功能：**内容过滤订阅**（content filtering subscription）只接收满足条件的消息；**键控话题**（keyed topic）在一个话题里区分多个对象实例；**等待确认**（wait for acknowledgment）让发布者确认订阅者已经收到。📄

## 2. 为什么需要

| 功能 | 解决的问题 | 导航里的例子（💡） |
|---|---|---|
| 内容过滤 | 订阅者只关心一部分数据，不想在回调里自己丢弃 | 只要电量低于 20% 的电池状态；只要置信度高于阈值的检测 |
| 键控话题 | 很多同类对象（10 个传感器、20 台机器人）如果各开一个话题，资源开销大；共用一个普通话题又会互相覆盖状态 | 车队里每台机器人的状态放在一个话题，按 `robot_id` 区分 |
| 等待确认 | 发布/订阅默认不确认送达 | 退出前确认"急停解除"或最后一条配置消息已被收到 |

### RMW 支持情况 📄

| 功能 | rmw_fastrtps | rmw_connextdds | rmw_cyclonedds | rmw_zenoh |
|---|---|---|---|---|
| 内容过滤 | 支持 | 支持 | 不支持 | 不支持 |
| 键控话题 | 支持 | 支持 | 不支持 | 文档未列 |
| 等待确认 | 支持 | 支持 | 支持 | 文档未列 |

- 内容过滤的表达式语法来自 DDS 规范附录 B（类似 SQL 的 WHERE 子句）。📄
- 不支持键控话题的实现会把它当普通话题处理；而且 Cyclone DDS 的端点和 Fast DDS / Connext 的端点**在键控话题上根本不会匹配**。📄
- 等待确认要求发布端 QoS 为 **Reliable**，否则不会等。📄
- 🔍 Rolling 镜像默认 RMW 是 `rmw_fastrtps_cpp`（`ros2 doctor --report` 实测），三个功能默认都可用。

## 3. 最小 C++ 代码（rclcpp）

**内容过滤订阅**（📄 摘自文档 demo，温度超出 [-30, 100] 才回调）：

```cpp
rclcpp::SubscriptionOptions sub_options;
sub_options.content_filter_options.filter_expression = "data < %0 OR data > %1";
sub_options.content_filter_options.expression_parameters = {
  std::to_string(-30.0f), std::to_string(100.0f)};

sub_ = create_subscription<std_msgs::msg::Float32>("temperature", 10, callback, sub_options);

if (!sub_->is_cft_enabled()) {
  // 当前 RMW 不支持：会收到全部消息，回调里要自己再判断一次
  RCLCPP_WARN(get_logger(), "content filter is not enabled");
}
```

**键控消息**：目前只有 `.idl` 文件能写 `@key` 注解，`.msg`、`.srv` 还不支持。📄 可以用 `ros2 run rosidl_adapter msg2idl.py X.msg` 把 msg 转成 idl 再加注解。📄

```idl
// RobotState.idl
module fleet_msgs {
  module msg {
    struct RobotState {
      @key long robot_id;   // 按 robot_id 区分实例
      double x;
      double y;
    };
  };
};
```

**等待确认**：发布端（Reliable）调用 `wait_for_all_acked(timeout)`，返回是否所有订阅者都确认了。📄 🔍 Rolling 的 `publisher_base.hpp` 里它返回 `bool`：确认完返回 `true`，超时返回 `false`，其他错误抛异常；不传超时参数时默认 -1（一直等）。 文档示例 `examples_rclcpp_minimal_publisher` 的 `member_function_with_wait_for_all_acked.cpp` 在退出前调用它。

```cpp
// 例如在析构或关闭流程中
if (publisher_->wait_for_all_acked(std::chrono::seconds(1))) {
  RCLCPP_INFO(get_logger(), "All subscribers acknowledge messages");
}
```

## 4. 常见坑

1. **以为过滤一定生效。** 📄 不支持的 RMW 上订阅照常创建，只是不过滤。💡 用 `is_cft_enabled()` 检查，回调里保留一份同样的判断作为兜底。
2. **键控话题跨 RMW 混用。** 📄 Cyclone 端点和 Fast DDS / Connext 端点不会匹配。💡 车队里所有机器人必须用同一种 RMW。
3. **在 BestEffort 发布者上等确认。** 📄 不会等待。
4. **把等待确认当成"处理完成"。** 💡 确认只表示中间件层收到了样本，不表示订阅者的回调已经处理完。需要"处理完成"语义请用服务或动作。
5. **仓库教程的下载链接已失效。** 🔍 `Topic-Keys-Tutorial.rst` 里 `wget` 的地址还指向旧路径 `source/Tutorials/Advanced/Topic-Keys/resources/...`，2026-11-17 访问返回 404。示例文件其实就在本仓库 `source/ROS-Framework/interfaces/topics/Working-with-topics/Topic-Keys/resources/` 下，直接复制即可。

## 5. 与其他概念的关系

- [话题](topic.md)、[QoS](qos.md)：等待确认依赖 Reliable
- [接口定义](interface-definition.md)：键控消息要用 `.idl`
- 中间件与 RMW（第 7 批）：功能是否可用取决于 RMW
- 上一级：[总览脑图](../../map/overview.md)

## 6. 自测题（先自己答，再看[答案](../../answers/topic-advanced-answers.md)）

1. 你的系统现在用 Cyclone DDS，同事建议用内容过滤订阅把"低电量"告警之外的电池消息过滤掉，以节省带宽。在不换 RMW 的前提下会发生什么？你会怎样写这个订阅，才能在两种 RMW 下都正确？
2. 一个车队管理节点要接收 20 台机器人的状态。比较三种方案：每台一个话题、所有机器人共用一个普通话题、共用一个键控话题。各自的问题或优势是什么？（提示：想想 TransientLocal + KeepLast(1) 时，后加入的订阅者能拿到什么。）
