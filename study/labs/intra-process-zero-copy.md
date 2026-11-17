# 练习：进程内通信与零拷贝

> 适用：ROS 2 **Rolling**（rclcpp 33.2.0，默认 `rmw_fastrtps_cpp`）。对应卡片：[进程内通信与零拷贝](../cards/concurrency/intra-process.md)
> 代码：[ipc_lab.cpp](perf_lab/src/ipc_lab.cpp)、[loan_lab.cpp](perf_lab/src/loan_lab.cpp)
> 用时：约 25 分钟

## 0. 编译

同[组件练习](component-container.md)。

`ipc_lab` 在一个进程里创建一个节点、一个发布者和若干订阅者（话题 `n`，`std_msgs/Int32`），每 200 ms 发布一次，共约 1 秒。发布端和每个订阅端都打印消息的地址。

## 1. 地址变不变

```bash
ros2 run perf_lab ipc_lab --case <名字>
```

| `--case` | 设置 | 预测：订阅端地址和发布端相同吗 | 实测 |
|---|---|---|---|
| `one-unique` | 发 `unique_ptr`，1 个 `UniquePtr` 订阅者 | | |
| `two-unique` | 2 个 `UniquePtr` 订阅者 | | |
| `two-shared` | 2 个 `ConstSharedPtr` 订阅者 | | |
| `shared-and-unique` | 1 个 `ConstSharedPtr` + 1 个 `UniquePtr` | | |
| `by-ref` | `publish(*msg)`，1 个 `UniquePtr` 订阅者 | | |
| `ipc-off` | 节点不开进程内通信 | | |
| `keep-all` | QoS 用 `KeepAll` | | |

比较时要对齐**同一个值**（`got 2` 对 `pub 2`），不要只看相邻两行。

<details>
<summary>参考结果（填完预测再展开）</summary>

2026-11-17 在 `ros:rolling-ros-base` 里实测（地址每次运行都不同，只看是否相同）：

```text
=== one-unique
pub   0 at 0x561f8d71b920
  sub0(unique) got 0 at 0x561f8d71b920          # 相同
=== two-unique
pub   0 at 0x562f14008ac0
  sub1(unique) got 0 at 0x562f14009b10          # 副本
  sub0(unique) got 0 at 0x562f14008ac0          # 原件
=== two-shared
pub   0 at 0x556057044b40
  sub1(shared) got 0 at 0x556057044b40          # 相同
  sub0(shared) got 0 at 0x556057044b40          # 相同
=== shared-and-unique
pub   0 at 0x564687482d30
  sub0(shared) got 0 at 0x564687483da0          # 副本
  sub1(unique) got 0 at 0x564687482d30          # 原件
=== by-ref
pub   0 at 0x5619b6991910
  sub0(unique) got 0 at 0x5619b69918f0          # 副本
=== ipc-off
pub   0 at 0x556027d16110
  sub0(unique) got 0 at 0x556027d17140          # 副本
=== keep-all
exception: intraprocess communication on topic 'n' allowed only with keep last history qos policy
```

要点：
- 一对多时，`UniquePtr` 订阅者里只有一个能拿到原件；都用 `ConstSharedPtr` 才能全部零拷贝。
- 混用时 `UniquePtr` 拿原件，`ConstSharedPtr` 那边拿副本。
- 分配器会复用刚释放的内存，所以 `one-unique` 里连续几条消息的地址在两三个值之间来回。这不代表复制，只能比较同一条消息。
- 所有情形里第一条消息（值 0）都收到了。📄 文档的 `two_node_pipeline` 演示里第一条常常丢，是因为它的两个节点在发布前订阅还没建立好；本练习先创建订阅再创建发布者。

</details>

## 2. 借用消息

```bash
ros2 run perf_lab loan_lab
ROS_DISABLE_LOANED_MESSAGES=1 ros2 run perf_lab loan_lab
```

| | `Float64` 能借吗 | `String` 能借吗 | 实测 |
|---|---|---|---|
| 默认 | | | |
| `ROS_DISABLE_LOANED_MESSAGES=1` | | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

```text
# 默认
[INFO] [rclcpp]: Currently used middleware can't loan messages. Local allocator will be used.
rmw: rmw_fastrtps_cpp
Float64 can_loan_messages: true
String  can_loan_messages: false
Float64 loan #0 at 0x558d5d2e0f60
Float64 loan #1 at 0x558d5d2e0f60      # 中间件复用同一块共享内存

# ROS_DISABLE_LOANED_MESSAGES=1
(上面的 INFO 打印了两次)
Float64 can_loan_messages: false
String  can_loan_messages: false
```

- Fast DDS 能借定长的 `Float64`，不能借含 `string` 的 `String`。💡 默认时那条 INFO 只打印一次，禁用后打印两次，可以推断它是在每个不能借用的发布者创建时打印的。
- 两种情况下代码都正常运行，`borrow_loaned_message()` 在不能借时退回到本地分配器。

</details>

## 3. `uint8[]` 在 Rolling 里是什么类型

新建一个 `.cpp`，包含 `sensor_msgs/msg/point_cloud2.hpp`，逐条试下面的写法能不能编译：

```cpp
sensor_msgs::msg::PointCloud2 m;
std::vector<uint8_t> & v = m.data;              // (1)
std::vector<uint8_t> copy = m.data;             // (2)
m.data = std::vector<uint8_t>(10, 1);           // (3)
std::vector<uint8_t> * p = &m.data;             // (4)
template<class T> void f(std::vector<T> &);
f(m.data);                                      // (5)
auto d = m.data;                                // (6) d 是 std::vector 吗？
```

<details>
<summary>参考结果</summary>

🔍 实测：(1)(2)(3) 能编译；(4)(5) 编译失败；(6) 能编译，但 `d` 的类型是 `rosidl::Buffer<uint8_t>`，不是 `std::vector`。原因见卡片坑 6。

</details>

## 挑战（选做）

1. 在 `ipc_lab` 里加一个 `--case two-ref`：两个 `const Msg &` 回调。📄 文档说它和 `ConstSharedPtr` 行为相同，验证一下。
2. 用[组件练习](component-container.md)里的 `CloudFilter`，比较 4 MB 点云在 `callback:=unique` 和 `callback:=shared` 下的延迟有没有差别。
