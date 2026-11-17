---
tags: [ros2, cli, ros2doctor, daemon, 命令行, 调试]
distro: rolling
status: batch-6
aliases: [命令行工具, ros2 CLI, ros2 topic, ros2 doctor, ros2doctor, ros2 daemon, topic hz, topic bw, topic info -v, yaml-file, get_topic_names_and_types, get_publishers_info_by_topic]
---

# 命令行工具：ros2 CLI 与 ros2 doctor

> 适用：ROS 2 **Rolling**（2026-11-17 在 `ros:rolling-ros-base` 容器里实测，默认 RMW `rmw_fastrtps_cpp`）
> 来源标记：📄 仓库文档原文 · 🔍 源码或实测核实 · 💡 经验补充（可信，但请自己验证）
> 源文档：[About-Command-Line-Tools.rst](../../../source/Developer-Tools/Introspection-and-analysis/About-Command-Line-Tools.rst)、[Getting-Started-With-Ros2doctor.rst](../../../source/ROS-Framework/client-libraries/Working-with-Client-Libraries/Getting-Started-With-Ros2doctor.rst)、[Publishing-Messages-Using-YAML-Files.rst](../../../source/Developer-Tools/Introspection-and-analysis/Publishing-Messages-Using-YAML-Files.rst)

## 1. 一句话定义

`ros2` 命令是一组**临时加入 ROS 图的小节点**：`ros2 topic echo` 就是一个订阅者，`ros2 topic pub` 就是一个发布者，`ros2 node info` 查的是发现（discovery）得到的图信息。`ros2 doctor` 在这之上做一组体检：平台、网络、RMW、包版本、"有发布没订阅"等。📄

## 2. 为什么需要

💡 规划控制调试的第一步几乎都是用 CLI 回答三个问题：**话题在吗？数据对吗？频率够吗？**

| 想知道 | 命令 | 备注 |
|---|---|---|
| 图里有哪些节点、话题 | `ros2 node list`、`ros2 topic list -t` | `-t` 带类型 |
| 某个节点收发什么 | `ros2 node info /fake_planner` | 列出订阅、发布、服务、动作 |
| 谁在发、用什么 QoS | `ros2 topic info -v /map` | QoS 不匹配排查的主力（见 [QoS](../communication/qos.md)） |
| 数据内容 | `ros2 topic echo /plan --once`、`--field pose.position.x` | `--field` 只看一个字段 |
| 频率、带宽 | `ros2 topic hz /plan`、`ros2 topic bw /plan` | 🔍 `/plan` 2 Hz、每条 1.47 KB |
| 手动发一条 | `ros2 topic pub --times 1 /goal_pose ...` | 或 `--yaml-file` 发多条 |
| 环境有没有问题 | `ros2 doctor`、`ros2 doctor --report` | 见下 |

### daemon：为什么 CLI 有时"看不见"新节点

📄 `ros2` 命令默认先问后台的 **ROS 2 daemon**：它一直挂在图上，缓存发现信息，CLI 就不用每次从头发现。🔍 实测（同一容器）：

| 情况 | `ros2 topic list` 用时 |
|---|---|
| daemon 已停，第一次运行（顺带启动 daemon） | 1.0 s |
| `--no-daemon` | 1.0 s |
| daemon 在运行 | 0.37 s |

💡 daemon 按 `ROS_DOMAIN_ID` 和 RMW 区分（进程参数里能看到 `--ros-domain-id 0 --rmw-implementation rmw_fastrtps_cpp`）。改了网络或发现相关的环境变量后结果"不对"，先 `ros2 daemon stop` 再试，或者加 `--no-daemon` 对照。

### ros2 doctor 怎么读

📄 只有警告时仍然打印 `All <n> checks passed`；出现 `UserWarning: ERROR:` 才算检查失败。🔍 对 `fake_planner` 运行的结果：

```text
UserWarning: Cannot find the latest versions of packages: observe_lab. ...
UserWarning: Subscriber without publisher detected on /goal_pose.
UserWarning: Publisher without subscriber detected on /map.
UserWarning: Publisher without subscriber detected on /plan.
...
All 5 checks passed
```

💡 "有发布没订阅"在开发阶段很正常（没开 RViz 而已）；它真正有用的时候是**订阅者以为有人发，实际上话题名拼错了**：比如订阅了 `goal_pose`，发布方在 `/move_base_simple/goal`，doctor 会在两边各报一条。`--report` 输出平台、RMW、网络接口、每个话题的发布/订阅数，适合贴进问题报告。

## 3. 最小 C++ 代码：在节点里查询 ROS 图

CLI 能查的东西，节点里也能查。典型用途：启动时确认上游话题存在、QoS 兼容。完整代码：[graph_probe.cpp](../../labs/observe_lab/src/graph_probe.cpp)。

```cpp
auto node = std::make_shared<rclcpp::Node>("graph_probe");
// 发现是异步的：刚 init 时图几乎是空的，要等（真实代码里应循环等待 + 超时）
std::this_thread::sleep_for(std::chrono::seconds(2));

for (const auto & [name, types] : node->get_topic_names_and_types()) {   // ≈ ros2 topic list -t
  RCLCPP_INFO(node->get_logger(), "topic %s [%s]", name.c_str(), types.front().c_str());
}
node->count_publishers("/map");                                           // ≈ ros2 topic info
for (const auto & info : node->get_publishers_info_by_topic("/map")) {    // ≈ ros2 topic info -v
  const auto & qos = info.qos_profile();
  // info.node_name()、qos.reliability()、qos.durability()、qos.depth() ...
}
```

🔍 输出：`/map: 1 publishers, 0 subscribers`，`publisher fake_planner: reliability=reliable durability=transient_local depth=1`。`count_publishers("map")`（相对名）也能工作，会按节点命名空间展开。

## 4. 常见坑

1. **`ros2 topic echo` 会自动适配发布者的 QoS，你的节点不会。** 🔍 `/map` 是 transient local，`fake_planner` 启动 3 秒后再 `ros2 topic echo --once /map` 依然收到地图；加 `--qos-durability volatile` 就一直等不到（`timeout` 退出码 124）。💡 所以"CLI 能收到、我的节点收不到"多半是 QoS 问题，用 `ros2 topic info -v` 对比两边。
2. **`--yaml-file` 的发布顺序就是文件顺序。** 📄 文档示例 YAML 写的是 x=1、2、3，示例输出却是 1、3、2。🔍 实测 `goals.yaml`（x=1、2、3）按 1、2、3 发出，规划器日志依次是 `new goal (1.0, 0.0)`、`(2.0, 0.0)`、`(3.0, 0.0)`。文档输出是笔误。
3. **`--times` / `--once` 会等订阅者。** 🔍 `ros2 topic pub -h`：用 `--once`/`--times` 时 `-w` 默认 1，会打印 `Waiting for at least 1 matching subscription(s)...` 直到有匹配的订阅者；`-r` 连续发布时默认 0，不等。💡 脚本里用 `--times` 给不存在的话题发消息会一直卡住，外面加 `timeout`。
4. **`hz` 和 `bw` 的数字只代表 CLI 这一端。** 💡 它们是一个普通订阅者测出来的：Wi-Fi 上跨机器测，掉包和延迟都算在里面；大消息（点云）时 Python 反序列化本身可能成为瓶颈，测出来的频率偏低。和节点内部日志或 tracing 对照着看。
5. **Rolling 里会多出 `<话题>/_buf_cpu` 隐藏话题。** 🔍 `ros2 topic list --include-hidden-topics` 里出现 `/plan_markers/_buf_cpu`，节点名 `_NODE_NAME_UNKNOWN_`、类型哈希 `INVALID`；它来自 `rmw_fastrtps_cpp` 新的 `rosidl_buffer` 缓冲后端机制（库里有 `/_buf_cpu` 字符串和 `BufferEndpointRegistry`）。`ros2 doctor` 和 `get_topic_names_and_types()` 都会把它算进去。本例只有 `MarkerArray` 话题有，原因未深究。💡 看到不认识的 `_` 开头话题先别慌，它们是基础设施。
6. **`pkill -f 'topic pub'` 会把自己也杀掉。** 🔍 在 `bash -c "...; pkill -f 'topic pub'; ... ros2 topic pub ..."` 里，`bash -c` 的命令行本身就含这串字，被一起杀掉（退出码 143）。写成 `pkill -f '[t]opic pub'`，并且别在同一条命令行里再出现原字符串。

## 5. 与其他概念的关系

- [节点与 ROS 图](../communication/node.md)：CLI 查的就是图；节点名、命名空间规则相同
- [QoS](../communication/qos.md)：`topic info -v` 看 QoS；`echo` 自动适配 QoS 的行为是常见误判来源
- [参数](../configuration/parameter.md)：`ros2 param` 系列
- [服务与动作内省](service-introspection.md)：`ros2 service echo`、`ros2 action echo`
- [rosbag 录制回放](rosbag.md)：CLI 看"现在"，bag 看"刚才"
- 第 7 批"Domain ID 与发现"：daemon、发现范围、多机时 CLI 看不到节点
- 练习：[命令行与服务内省](../../labs/cli-and-introspection.md)
- 上一级：[总览脑图](../../map/overview.md)

## 6. 自测题（先自己答，再看[答案](../../answers/cli-tools-answers.md)）

1. 你的局部规划器订阅 `/map` 一直收不到数据，可同事说"`ros2 topic echo /map` 明明能看到"。根据本卡片，最可能的原因是什么？用哪条命令、看哪几行来确认？
2. 写一个 shell 脚本，启动机器人后自动给 `/goal_pose` 依次发三个目标点，每个间隔 1 秒，并且在没有订阅者时 10 秒内失败退出而不是卡住。写出关键命令，并说明 `--times`、`-r`、`-w` 各起什么作用。
