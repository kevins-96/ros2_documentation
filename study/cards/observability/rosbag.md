---
tags: [ros2, rosbag2, mcap, 录制, 回放, 调试]
distro: rolling
status: batch-6
aliases: [rosbag, rosbag2, ros2 bag, bag 录制, 回放, mcap, ros2 bag record, ros2 bag play, ros2 bag info, rosbag2_cpp::Writer, rosbag2_cpp::Reader, qos-profile-overrides-path, SerializedMessage]
---

# rosbag 录制回放

> 适用：ROS 2 **Rolling**（2026-11-17 在 `ros:rolling-ros-base` 容器里实测，存储格式默认 MCAP）
> 来源标记：📄 仓库文档原文 · 🔍 源码或实测核实 · 💡 经验补充（可信，但请自己验证）
> 源文档：[Recording-And-Playing-Back-Data.rst](../../../source/ROS-Framework/interfaces/Working-with-interfaces/Recording-And-Playing-Back-Data/Recording-And-Playing-Back-Data.rst)、[Recording-A-Bag-From-Your-Own-Node-CPP.rst](../../../source/ROS-Framework/client-libraries/Working-with-Client-Libraries/Recording-A-Bag-From-Your-Own-Node-CPP.rst)、[Reading-From-A-Bag-File-CPP.rst](../../../source/ROS-Framework/client-libraries/Working-with-Client-Libraries/Reading-From-A-Bag-File-CPP.rst)、[Overriding-QoS-Policies-For-Recording-And-Playback.rst](../../../source/Developer-Tools/Debugging/Testing/Overriding-QoS-Policies-For-Recording-And-Playback.rst)
> 📐 设计动机：[时钟与时间的设计](../../design/time-design.md)：`--clock` 回放、变速和 `--loop` 时下游节点看到的时间怎么变

## 1. 一句话定义

rosbag 把话题（以及开了内省的服务、动作）上的**序列化消息连同时间戳**存进一个 bag 目录（一个 `metadata.yaml` + 若干存储文件），之后可以按原来的时间节奏重新发布出来。📄

## 2. 为什么需要

💡 规划控制的 bug 往往只在某一段数据上出现："机器人在那个拐角抖了一下"。录下当时的 `/map`、`/odom`、`/scan`、`/tf`、`/plan`，就能在桌面上反复回放、换参数重跑规划器、对比修改前后的输出，不用每次都上车。

### 一个 bag 里有什么

🔍 `ros2 bag record -o b1 --topics /map /plan /plan_markers` 录 4 秒后：

```text
b1/
├── 0_b1_2026_11_17-16_09_55.mcap     # 数据文件（Rolling 的命名：序号_bag名_时间）
└── metadata.yaml                      # 话题列表、类型、条数、录制时的 QoS（offered_qos_profiles）
```

```text
Duration:          2.974691233s          # 第一条到最后一条消息，不是你录了多久
Messages:          13
Topic information: Topic: /map | Type: nav_msgs/msg/OccupancyGrid | Count: 1 | ...
                   Topic: /plan | Type: nav_msgs/msg/Path | Count: 6 | ...
```

📄 Humble 及以前默认 sqlite3（`.db3`），之后默认 MCAP。📄 分割文件命名在 Lyrical 改成 `0_name_....mcap`（以前是 `name_0.mcap`）；🔍 Rolling 里 `-d 2` 录 5.5 秒得到 `0_b2_<时间>.mcap`、`1_b2_<时间>.mcap`。

### 常用命令（🔍 均在 Rolling 实测）

| 目的 | 命令 |
|---|---|
| 录指定话题 | `ros2 bag record -o run1 --topics /map /odom /plan`（**Rolling 必须写 `--topics`**，见常见坑 1） |
| 录全部 | `-a`：🔍 话题 + 服务事件都录，还会带上 `/rosout`、`/parameter_events`、`/events/write_split` 等 |
| 按时长 / 大小分割 | `-d 60`（秒）、`-b 100000000`（字节） |
| 看内容 | `ros2 bag info run1` |
| 回放 | `ros2 bag play run1`；`--rate 2` 两倍速（🔍 2 Hz 话题回放成 4 Hz）；`--loop`；`--topics /plan` 只放部分 |
| 回放时发布时钟 | `--clock`：🔍 发布 `/clock`（best effort、volatile），节点配 `use_sim_time:=true` 才跟着 bag 时间走 |
| 多个 bag 一起放 | 📄 `ros2 bag play -i bag_a -i bag_b`，按时间戳合并 |
| 录服务 | 📄 服务端或客户端先开内省，再 `--services /replan` 或 `--all-services`（Jazzy 起）；回放时 `--publish-service-requests` 把请求重新发给服务 |
| 录动作 | 📄 `--actions`（Kilted 起，同样要开内省） |
| 出事后才存 | 🔍 `--snapshot-mode`：消息先留在内存缓冲，调用服务 `/rosbag2_recorder/snapshot`（`rosbag2_interfaces/Snapshot`）才写盘（来自 Rolling 的 `ros2 bag record -h`，本次未实测） |
| 分割后每个文件都带地图 | 🔍 `--repeat-transient-local /map`（或 `--repeat-all-transient-local`）：分割、snapshot 时把 transient local 话题的最后几条补写进新文件。实测 `-d 2` 分割：不加时只有第一个文件有 `/map`，加了之后 3 个文件各有 1 条 |

### QoS：rosbag 自己会适配

📄 rosbag 按发现到的发布者 QoS 调整自己的订阅和回放 QoS，避免丢数据；特殊情况才需要 `--qos-profile-overrides-path` 覆盖。🔍 Rolling 实测：`/map`（transient local）在录制开始**前 3 秒**就发完了，bag 里照样有这 1 条；回放时 `rosbag2_player` 的 `/map` 发布者也是 `TRANSIENT_LOCAL`，回放开始 1.5 秒后才启动的 `ros2 topic echo` 也收到了地图（读的是 `metadata.yaml` 里记下的 `offered_qos_profiles`）。

覆盖文件按话题名写（📄 时长类策略要同时写 `sec` 和 `nsec`）：

```yaml
# observe_lab/config/map_qos_override.yaml
/map:
  durability: transient_local
  reliability: reliable
  history: keep_last
  depth: 1
```

💡 什么时候需要覆盖：同一话题多个发布者 QoS 不一致（rosbag 选不出统一的）；或者你想故意用和录制时不同的 QoS 回放。

## 3. 最小 C++ 代码

### 在自己的节点里写 bag（不经过反序列化）

完整代码：[plan_recorder.cpp](../../labs/observe_lab/src/plan_recorder.cpp)

```cpp
#include "nav_msgs/msg/path.hpp"
#include "rosbag2_cpp/writer.hpp"

writer_ = std::make_unique<rosbag2_cpp::Writer>();
writer_->open("plan_bag");                 // 目录已存在会抛异常
// 模板参数写真实类型（决定订阅哪种话题），回调参数写 SerializedMessage（拿原始字节）
sub_ = create_subscription<nav_msgs::msg::Path>(
  "/plan", 10, [this](std::shared_ptr<const rclcpp::SerializedMessage> msg) {
    writer_->write(msg, "/plan", "nav_msgs/msg/Path", now());   // 话题名原样写进 bag
  });
// writer_ 析构时关闭存储并写 metadata.yaml
```

CMake：`target_link_libraries(... rosbag2_cpp::rosbag2_cpp ${nav_msgs_TARGETS})`，package.xml `<depend>rosbag2_cpp</depend>`。

### 不回放，直接读 bag 做统计

完整代码：[plan_bag_reader.cpp](../../labs/observe_lab/src/plan_bag_reader.cpp)

```cpp
#include "rclcpp/serialization.hpp"
#include "rosbag2_cpp/reader.hpp"

rosbag2_cpp::Reader reader;
reader.open(argv[1]);                                  // bag 目录
rclcpp::Serialization<nav_msgs::msg::Path> serialization;
while (reader.has_next()) {
  auto bag_msg = reader.read_next();                   // SerializedBagMessage
  if (bag_msg->topic_name != "/plan") continue;
  rclcpp::SerializedMessage raw(*bag_msg->serialized_data);
  nav_msgs::msg::Path path;
  serialization.deserialize_message(&raw, &path);
  // bag_msg->recv_timestamp：录制时收到的时间（ns）；path.header.stamp：消息自己的时间
}
```

🔍 对 `ros2 bag record` 录的 b1：`/plan: 6 messages over 2.50 s, 20.0 points on average`，`first message: bag time - header.stamp = 0.338 ms`。

💡 离线统计（路径长度、曲率、控制误差）用 Reader 比"回放 + 订阅"快得多，也不受实时性影响，适合批量回归测试。

## 4. 常见坑

1. **Rolling 的 `ros2 bag record` 不再接受位置参数话题。** 🔍 `ros2 bag record -o b1 /map /plan` 报 `ros2: error: unrecognized arguments: /map /plan`，必须写 `--topics /map /plan`。📄 文档的命令已经都用 `--topics`；旧博客、旧脚本里的写法会失败。
2. **`create_subscription<rclcpp::SerializedMessage>` 编译通过、运行时崩溃。** 🔍 报 `terminate called after throwing an instance of 'std::runtime_error' what(): this specialization of rclcpp::get_message_type_support_handle() should never be called`。模板参数必须是真实消息类型，只有回调参数写 `SerializedMessage`（📄 文档示例就是这样写的）。不知道类型时用 `create_generic_subscription(topic, type_string, qos, cb)`。
3. **进程崩溃时 bag 没有 `metadata.yaml`。** 🔍 上一条崩溃后目录里只有 `.mcap`，`ros2 bag info` 报 `Could not find metadata in bag directory`，`Reader::open` 抛 `No storage could be initialized`。`ros2 bag reindex` 也没能救回来（这次崩溃发生在写入任何消息前）。💡 `Writer` 析构才写元数据：节点要能正常退出（SIGINT 后 `rclcpp::shutdown()`，让 `unique_ptr<Writer>` 析构）。
4. **在自己的节点里写 bag，话题名是什么就记什么。** 🔍 `write(msg, "plan", ...)` 写进 bag 的话题叫 `plan`（不带 `/`），按 `/plan` 过滤一条都找不到；回放时播放器把 `plan` 解析成 `/plan` 发布，看起来又是对的。统一写完整名字。
5. **bag 目录已存在就无法录制。** 🔍 `Bag directory already exists (cpp_bag2), can't overwrite existing bag`（CLI 和 `Writer::open` 一样）。💡 脚本里用带时间戳的名字，或者先检查。
6. **`Duration` 不是录制时长，回放从第一条消息开始。** 🔍 录了 4 秒，`Duration: 2.97s`。低频话题（1 Hz 的地图更新）开头那段空白不会被回放。
7. **脚本里后台启动的 `ros2 bag record` 收不到 SIGINT。** 🔍 非交互 bash 里用 `&` 启动的进程，SIGINT 被设为忽略，`kill -INT` 没反应、`wait` 永远等下去。脚本开头加 `set -m`（开启作业控制）后正常停止并打印 `Recording stopped`。💡 也可以发 SIGTERM。
8. **回放时还在用墙钟时间。** 💡 bag 里的 `header.stamp` 是录制那天的时间；节点不开 `use_sim_time`、回放不加 `--clock`，tf2 查询会因为时间差报外推错误（见 [tf2 与时间](../coordinates/tf2-time.md)）。

## 5. 与其他概念的关系

- [QoS](../communication/qos.md)：rosbag 自动适配 QoS；transient local 话题录制前发出的消息也能录到
- [tf2 与时间](../coordinates/tf2-time.md)：回放 + `--clock` + `use_sim_time` 才能让 tf2 查询对得上
- [服务与动作内省](service-introspection.md)：录服务、动作的前提
- [命令行工具](cli-tools.md)：录之前先用 `ros2 topic list -t`、`hz` 确认话题和频率
- [测试](../engineering/testing.md)：Reader 读 bag 做离线回归测试
- 练习：[录制、回放与离线分析](../../labs/bag-record-replay.md)
- [时钟与时间的设计](../../design/time-design.md)（📐 设计动机）：回放循环时的时间跳变
- 上一级：[总览脑图](../../map/overview.md)

## 6. 自测题（先自己答，再看[答案](../../answers/rosbag-answers.md)）

1. 你在车上录了 `ros2 bag record -o run1 --topics /scan /odom /tf /tf_static /plan`，回到桌面回放，同时启动自己的局部规划器，日志全是 `Lookup would require extrapolation into the past`。说出最可能的两个原因和对应的改法。
2. 你想在规划器节点里"出错时把最近 10 秒的 `/plan` 和 `/odom` 存成 bag"。用本卡片的 API，说明消息怎么存、出错时怎么写盘、要注意哪两个坑（提示：消息类型、元数据）。
