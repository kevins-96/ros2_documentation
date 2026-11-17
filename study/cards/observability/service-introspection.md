---
tags: [ros2, service, action, introspection, 内省, 调试]
distro: rolling
status: batch-6
aliases: [服务内省, 动作内省, service introspection, action introspection, ros2 service echo, ros2 action echo, configure_introspection, _service_event, RCL_SERVICE_INTROSPECTION_CONTENTS, ServiceEventInfo]
---

# 服务与动作内省

> 适用：ROS 2 **Rolling**（2026-11-17 在 `ros:rolling-ros-base` 容器里实测服务部分；动作部分只核对了文档）
> 来源标记：📄 仓库文档原文 · 🔍 源码或实测核实 · 💡 经验补充（可信，但请自己验证）
> 源文档：[Service-Introspection.rst](../../../source/Developer-Tools/Introspection-and-analysis/Service-Introspection.rst)、[Action-Introspection.rst](../../../source/ROS-Framework/interfaces/actions/Working-with-actions/Action-Introspection.rst)、[Recording-And-Playing-Back-Data.rst](../../../source/ROS-Framework/interfaces/Working-with-interfaces/Recording-And-Playing-Back-Data/Recording-And-Playing-Back-Data.rst)（录制服务、动作部分）

## 1. 一句话定义

服务内省（service introspection）让服务端或客户端把每次调用的**事件**（收到请求、发出响应等）额外发布到一个隐藏话题 `<服务名>/_service_event` 上，于是 `ros2 service echo` 能"偷看"调用，rosbag 能录制服务。默认关闭，需要在代码里打开。📄

## 2. 为什么需要

话题可以随时 `echo`，服务调用却是点对点的：`/replan` 什么时候被谁调用、请求里写了什么、返回了什么，从外面看不见。💡 导航里大量交互走服务和动作（清代价地图、切换地图、`NavigateToPose`），出问题时想知道"到底有没有人调过、参数是什么"，就靠内省。

### 三种状态与四种事件

| 状态（📄） | 事件里有什么 | 🔍 `/replan`（`std_srvs/Trigger`）实测 |
|---|---|---|
| `RCL_SERVICE_INTROSPECTION_OFF` | 不发布事件 | 隐藏话题不存在；`ros2 service echo` 报 `No publishers on topic '/replan/_service_event'; is service introspection on the client or server enabled?` |
| `RCL_SERVICE_INTROSPECTION_METADATA` | 只有元数据：事件类型、时间戳、客户端 GID、序号 | `request: []`、`response: []` |
| `RCL_SERVICE_INTROSPECTION_CONTENTS` | 元数据 + 请求/响应内容 | `request: - {}`（Trigger 请求本来就是空的） |

📄 事件类型：`REQUEST_SENT`（客户端发出）、`REQUEST_RECEIVED`（服务端收到）、`RESPONSE_SENT`（服务端发出）、`RESPONSE_RECEIVED`（客户端收到）。🔍 只在服务端打开时，一次调用只看到 `REQUEST_RECEIVED` 和 `RESPONSE_SENT` 两条。💡 客户端和服务端都打开，才能看到完整的四条（按事件定义推断，本次没有写带内省的客户端来实测）。

```text
info:
  event_type: REQUEST_RECEIVED
  stamp: {sec: 1794931858, nanosec: 604316838}
  client_gid: [1, 15, 120, 253, ...]      # 哪个客户端调的
  sequence_number: 1                       # 同一客户端的第几次调用；配对请求和响应用
request: []
response: []
```

## 3. 最小 C++ 代码

完整代码：[fake_planner.cpp](../../labs/observe_lab/src/fake_planner.cpp) 的 `configure_introspection()`

```cpp
replan_srv_ = create_service<std_srvs::srv::Trigger>("replan", callback);

rcl_service_introspection_state_t state = RCL_SERVICE_INTROSPECTION_OFF;
if (mode == "metadata") {
  state = RCL_SERVICE_INTROSPECTION_METADATA;
} else if (mode == "contents") {
  state = RCL_SERVICE_INTROSPECTION_CONTENTS;
}
// 事件发布到隐藏话题 /replan/_service_event；时间戳用节点时钟（use_sim_time 时是仿真时间）
replan_srv_->configure_introspection(get_clock(), rclcpp::SystemDefaultsQoS(), state);
```

客户端同理：`client_->configure_introspection(get_clock(), qos, state)`。📄 文档示例把状态做成参数（`service_configure_introspection`），在参数回调里切换，运行时 `ros2 param set` 就能打开。

```bash
ros2 run observe_lab fake_planner --ros-args -p service_introspection:=contents
ros2 service echo /replan                     # 另一个终端
ros2 service call /replan std_srvs/srv/Trigger
ros2 topic list --include-hidden-topics | grep _service_event   # /replan/_service_event
```

### 录制服务

🔍 服务端开 `contents` 后 `ros2 bag record -o svc --services /replan`，调用两次：`ros2 bag info` 显示 `Services: 1`，`Service: /replan | Type: std_srvs/srv/Trigger | Event Count: 4`（两次调用 × 两个事件）。📄 回放时默认只把事件重新发布到 `_service_event`；加 `--publish-service-requests` 会把录到的请求重新**发给**服务（会真的触发服务端逻辑）。

### 动作内省（📄，Kilted 起）

动作由 3 个服务（发送目标、取消、取结果）和 2 个话题（反馈、状态）组成。📄 `action_server->configure_introspection(clock, qos, state)`、客户端同理；用 `ros2 action echo <动作名>` 查看，`ros2 bag record --actions <动作名>` 录制。文档示例的参数名是 `action_server_configure_introspection` / `action_client_configure_introspection`。

## 4. 常见坑

1. **默认关闭，而且要在被观察的那一方打开。** 🔍 `ros2 service echo` 本身打不开别人的内省；`ros2 service call` 发起的调用里客户端也没开内省，所以只看到服务端两条事件。💡 想排查第三方节点（比如 Nav2 的服务）时，先查它有没有暴露内省参数，没有就只能改代码或用 tracing。
2. **`metadata` 下看不到请求内容。** 🔍 `request: []`。要看参数就用 `contents`。💡 `contents` 会把每个请求和响应完整序列化再发一遍：大请求（比如带整条路径的服务）会多一份开销，现场长期开着之前要想一想。
3. **`client_gid` 是一串数字，不是节点名。** 🔍 只能用来区分"是不是同一个客户端"。想知道是谁调的，需要客户端也开内省并结合 `ros2 node info` 等信息推断。
4. **录服务之前必须已经打开内省。** 📄 bag 录的是 `_service_event` 话题，内省关着时什么也录不到；🔍 `ros2 bag record -a` 会顺带录服务事件（`Services: 1`），前提同样是内省已开。
5. **`--publish-service-requests` 会真的调用服务。** 📄 回放时把录到的请求发给当前在线的服务端。💡 对"清代价地图""切换地图"这类有副作用的服务，在真车上回放前要确认。

## 5. 与其他概念的关系

- [服务](../communication/service.md)、[动作](../communication/action.md)：被观察的对象；动作内部就是 3 个服务 + 2 个话题
- [rosbag 录制回放](rosbag.md)：`--services`、`--actions`、`--publish-service-requests`
- [命令行工具](cli-tools.md)：`ros2 service echo`、`--include-hidden-topics`
- [参数](../configuration/parameter.md)：把内省状态做成参数，运行时开关
- 练习：[命令行与服务内省](../../labs/cli-and-introspection.md)
- 上一级：[总览脑图](../../map/overview.md)

## 6. 自测题（先自己答，再看[答案](../../answers/service-introspection-answers.md)）

1. 地图服务器偶尔被切换到错误的地图，你怀疑有个节点在不该调用的时候调了 `/map_server/load_map`。设计一个排查方案：在哪一方打开内省、用什么状态、用什么工具记录，最后怎么找出是哪个客户端在什么时间调用的。
2. 同事把 `ros2 bag play --publish-service-requests` 加进了"回放复现问题"的脚本里。这样做在什么情况下有用？在什么情况下有风险？
