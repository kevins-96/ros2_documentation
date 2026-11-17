---
tags: [ros2, rclcpp, 进程内通信, 零拷贝, zero-copy, loaned message]
distro: rolling
status: batch-4
aliases: [进程内通信, intra-process, IPC, 零拷贝, zero copy, loaned message, 借用消息, use_intra_process_comms, rosidl::Buffer, Buffer Backend]
---

# 进程内通信与零拷贝 Intra-process & Zero-copy

> 适用：ROS 2 **Rolling**（2026-11-17 在 `ros:rolling-ros-base` 上编译实测，rclcpp 33.2.0，默认 RMW `rmw_fastrtps_cpp`）
> 来源标记：📄 仓库文档原文 · 🔍 源码或实测核实 · 💡 经验补充（可信，但请自己验证）
> 源文档：[Intra-Process-Communication.rst](../../../source/ROS-Framework/nodes/Working-with-nodes/intra-process/Intra-Process-Communication.rst)、[Configure-ZeroCopy-loaned-messages.rst](../../../source/ROS-Framework/client-libraries/Working-with-Client-Libraries/Configure-ZeroCopy-loaned-messages.rst)、[About-Buffer-Backends.rst](../../../source/ROS-Framework/interfaces/Working-with-interfaces/Buffer-Backends/About-Buffer-Backends.rst)、[Using-Buffer-Backends.rst](../../../source/ROS-Framework/interfaces/Working-with-interfaces/Buffer-Backends/Using-Buffer-Backends.rst)

## 1. 一句话定义

进程内通信（intra-process communication）让**同一进程**里的发布者和订阅者直接传递消息指针，不经过序列化和中间件；配合 `std::unique_ptr` 发布，消息可以做到**零拷贝**（zero-copy）。📄

## 2. 为什么需要

💡 一帧 64 线激光点云有几 MB，相机图像也有几 MB。感知 → 滤波 → 代价地图这条链上每一跳都复制一次，CPU 和延迟都扛不住。

ROS 2 有三种减少复制的机制，作用范围不同（📄）：

| 机制 | 范围 | 前提 |
|---|---|---|
| 进程内通信 | 同一进程 | 节点开 `use_intra_process_comms(true)`；发布 `unique_ptr` |
| 借用消息（loaned message） | 跨进程、同一主机（共享内存） | RMW 支持；消息是定长 POD 类型 |
| `rosidl::Buffer` 后端（Buffer Backend） | `uint8[]` 等变长数组字段放在 GPU 等非 CPU 内存 | 安装了对应后端插件；目前只支持话题 |

### 消息有没有被复制：看回调签名 📄 🔍

🔍 下表来自 [ipc_lab](../../labs/perf_lab/src/ipc_lab.cpp) 实测：比较同一条消息在发布端和订阅端的地址。

| 发布方式 | 订阅者 | 结果 |
|---|---|---|
| `publish(std::move(unique_ptr))` | 1 个 `UniquePtr` 回调 | **地址相同**，零拷贝 |
| 同上 | 2 个 `UniquePtr` 回调 | 一个拿到原件，另一个拿到**副本** |
| 同上 | 2 个 `ConstSharedPtr` 回调 | **地址都相同**，零拷贝 |
| 同上 | 1 个 `ConstSharedPtr` + 1 个 `UniquePtr` | `UniquePtr` 拿原件，`ConstSharedPtr` 拿副本 |
| `publish(const T &)` | 1 个 `UniquePtr` 回调 | **副本** |
| 节点没开进程内通信 | 1 个 `UniquePtr` 回调 | **副本**（走中间件） |

📄 规则：
- `UniquePtr` 回调拿到的是**可修改的独占所有权**，适合"收到后就地修改再发出去"的流水线。
- `ConstSharedPtr`（`const std::shared_ptr<const T> &`）和 `const T &` 回调共享同一个只读对象，**一对多也不复制**。`const T &` 在回调返回后就失效，不能保存。
- 可变的 `std::shared_ptr<T>` 回调已弃用：只有一个订阅者时零拷贝，多于一个就要复制。

## 3. 最小 C++ 代码

片段（完整可运行的版本见 [ipc_lab.cpp](../../labs/perf_lab/src/ipc_lab.cpp)）：

```cpp
// 两个节点在同一进程、同一执行器里，都开进程内通信
auto opts = rclcpp::NodeOptions().use_intra_process_comms(true);
auto producer = std::make_shared<rclcpp::Node>("producer", opts);
auto consumer = std::make_shared<rclcpp::Node>("consumer", opts);

auto pub = producer->create_publisher<sensor_msgs::msg::PointCloud2>("cloud", 10);
auto sub = consumer->create_subscription<sensor_msgs::msg::PointCloud2>(
  "cloud", 10,
  [](sensor_msgs::msg::PointCloud2::ConstSharedPtr msg) {   // 只读：一对多也不复制
    // ... 使用 msg->data
  });

auto msg = std::make_unique<sensor_msgs::msg::PointCloud2>();
msg->data.resize(4000000);
pub->publish(std::move(msg));   // 交出所有权；之后 msg 为空，不能再用
```

组件里不用改代码，在 launch 里给每个组件加 `<extra_arg name="use_intra_process_comms" value="true"/>`（见[组件与组合](component.md)）。

### 借用消息 📄

```cpp
auto loaned = pod_pub->borrow_loaned_message();   // 中间件支持时，内存由中间件分配
loaned.get().data = 1.0;
pod_pub->publish(std::move(loaned));              // 发布后借来的消息失效
```

- 📄 发布端默认启用；中间件不支持时退回到发布者自己的分配器，代码不用改。
- 📄 支持情况：`rmw_fastrtps` 支持，`rmw_connextdds`、`rmw_cyclonedds` 不支持。
- 📄 订阅端默认**关闭**（存在安全问题），要设 `ROS_DISABLE_LOANED_MESSAGES=0` 才开；发布端设 `ROS_DISABLE_LOANED_MESSAGES=1` 关闭。
- 🔍 实测（[loan_lab](../../labs/perf_lab/src/loan_lab.cpp)，Fast DDS）：`Float64` 发布者 `can_loan_messages()` 为 true，`String` 为 false，并打印 `Currently used middleware can't loan messages. Local allocator will be used.`；设 `ROS_DISABLE_LOANED_MESSAGES=1` 后两者都是 false。

### Rolling 的变化：`uint8[]` 不再是 `std::vector` 📄 🔍

📄 Rolling 里 `uint8[]`、`float32[]` 这类变长基本类型数组字段，生成的 C++ 类型从 `std::vector<T>` 变成了 `rosidl::Buffer<T>`，目的是让 GPU 等后端可以接管这块内存。默认是 CPU 后端，行为和 `std::vector` 一样，并能隐式转换成 `std::vector<T> &`。

🔍 镜像里 `sensor_msgs/msg/Image`、`PointCloud2`、`std_msgs/msg/UInt8MultiArray` 的 `data` 都已经是 `rosidl::Buffer<uint8_t, ...>`。下面这些写法实测**仍然能编译**：`resize`、`push_back`、`[]`、`data()`、范围 for、`std::fill`、`insert`、`memcpy`、`std::vector<uint8_t> & v = msg.data`、`std::vector<uint8_t> v = msg.data`、`msg.data = some_vector`、传给参数是 `std::vector<uint8_t> &` 的函数。见坑 6。

## 4. 常见坑

🔍 以下结果来自[进程内通信练习](../../labs/intra-process-zero-copy.md)的实测。

1. **按引用发布，白开了进程内通信。** 🔍 `publish(*msg)` 时订阅者收到的地址和发布的不同。📄 只有交出 `unique_ptr` 所有权，rclcpp 才敢不复制。
2. **一对多时用了 `UniquePtr` 回调。** 🔍 两个 `UniquePtr` 订阅者，有一个拿到的是副本。📄 哪个拿到原件是不确定的。只读处理（代价地图、可视化）用 `ConstSharedPtr`。
3. **混用两种回调也会复制。** 🔍 一个 `ConstSharedPtr` 加一个 `UniquePtr`：`UniquePtr` 拿原件，`ConstSharedPtr` 拿副本。
4. **`KeepAll` 和进程内通信不能同时用。** 🔍 开了进程内通信再用 `rclcpp::QoS(rclcpp::KeepAll())` 创建订阅，直接抛异常：`intraprocess communication on topic 'n' allowed only with keep last history qos policy`。
5. **只开了一端。** 💡 进程内通信要求发布者和订阅者所在的节点**都**开启，并且在同一个进程（同一个 `rclcpp::Context`）里。组件里每个 `composable_node` 都要单独加 `extra_arg`。
6. **从 Jazzy 等旧版本迁移的代码在 Rolling 上编译失败。** 🔍 实测这三种写法会失败：
   - 模板参数推导：`template<class T> void f(std::vector<T> &)` 调用 `f(msg.data)` 报 `no matching function`；
   - 取地址：`std::vector<uint8_t> * p = &msg.data` 报 `cannot convert ... rosidl::Buffer<unsigned char> * to std::vector ...`；
   - `auto d = msg.data` 得到的是 `rosidl::Buffer`，不是 `std::vector`，后面按 `std::vector` 使用会出问题。

   💡 改法：显式写 `std::vector<uint8_t> & v = msg.data;` 再用 `v`。📄 只有 CPU 后端才能隐式转换；收到非 CPU 后端的消息前要先判断 `msg->data.get_backend_type()`。
7. **以为进程间也能零拷贝。** 📄 进程内通信只在同一进程里有效。🔍 跨进程时一帧 1 MB 的点云在 SensorDataQoS 下 2 秒只收到 0~1 帧（详见[组件与组合](component.md)）。跨进程零拷贝要靠借用消息（共享内存）或 Buffer 后端，这两者都依赖 RMW。
8. **借用消息只对定长类型生效。** 🔍 `String` 不能借用，`Float64` 可以。📄 文档要求 Fast DDS 下借用的消息是 POD 类型。💡 所以 `PointCloud2`、`Image` 这类含变长数组的消息也借不了（未实测）。

## 5. 与其他概念的关系

- [组件与组合](component.md)：把节点放进同一进程的标准方式
- [话题](../communication/topic.md)、[QoS](../communication/qos.md)：进程内通信只支持 KeepLast
- [实时编程](realtime.md)：零拷贝减少了堆分配，自定义分配器也能传给进程内通信
- [接口定义](../communication/interface-definition.md)：`uint8[]` 在 Rolling 生成 `rosidl::Buffer`
- 中间件 RMW 与 DDS（第 7 批）：借用消息、共享内存、Buffer 后端都依赖 RMW
- 练习：[进程内通信与零拷贝](../../labs/intra-process-zero-copy.md)
- 上一级：[总览脑图](../../map/overview.md)

## 6. 自测题（先自己答，再看[答案](../../answers/intra-process-answers.md)）

1. 一个容器里有 `lidar_driver` → `voxel_filter` → 两个下游（`costmap_obstacle_layer` 和 `rviz_bridge`）。四个组件都开了进程内通信。`voxel_filter` 收到点云后要**就地**删掉一些点再发出去；两个下游只读。每个回调该用什么签名、`publish` 怎么写，才能让整条链一次复制都没有？
2. 同事说："我开了 `use_intra_process_comms`，但订阅者打印出来的地址和发布者的不一样，说明进程内通信没生效。"请列出至少三种可能的原因，以及怎么逐一排查。
