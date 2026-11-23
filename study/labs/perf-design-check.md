# 练习：并发与性能的设计在 Rolling 里还成立吗？

> 适用：ROS 2 **Rolling**（默认 RMW `rmw_fastrtps_cpp`）。对应设计笔记：[实时系统的设计](../design/realtime-design.md)、[进程内通信与零拷贝的设计](../design/intra-process-design.md)、[rmw 接口与序列化的设计](../design/rmw-serialization-design.md)
> 代码：[design_lab/](design_lab/)（本批新增 `rt_alloc_probe`、`ipc_design_probe`、`rmw_probe`），以及 [study_interfaces](study_interfaces/) 里新增的 `FixedBlock.msg`（1 MB 定长、没有字符串的消息）
> 用时：约 40 分钟

做法和前两批的练习（[第 8 批](design-claims-check.md)、[第 9 批](config-design-check.md)）一样：先填预测，再运行，最后展开参考结果。**预测错了的地方，就是你对设计理解有偏差的地方。**

## 0. 编译

```bash
ln -s <你的仓库路径>/study/labs/* ~/study_ws/src/
cd ~/study_ws && source /opt/ros/rolling/setup.bash
colcon build --packages-up-to design_lab && source install/setup.bash
```

或者用官方镜像：`docker run -it --rm -v <你的仓库路径>/study/labs:/ws/src/labs ros:rolling-ros-base`，在容器里执行 `cd /ws && colcon build --packages-up-to design_lab`。

`rmw_probe` 链接了 `rmw_fastrtps_cpp` 的扩展 API，所以 `design_lab` 现在依赖 `rmw_fastrtps_cpp` 和 `fastdds`（`ros-base` 镜像自带）。

## 1. 自定义分配器能挡住多少次系统分配

`rt_alloc_probe` 替换了全局 `operator new`，用来统计"系统分配器"被调用的次数；自定义分配器 `CountingAllocator` 直接调 `malloc`，单独计数。同一个节点里有一个 `Int32` 发布者和一个订阅者，每发一条就 `spin_some` 一次，预热 100 条之后测 1000 条。

```bash
ros2 run design_lab rt_alloc_probe --case default      # 默认分配器，按引用发布同一条消息
ros2 run design_lab rt_alloc_probe --case custom       # 发布者、订阅者、消息内存策略都用自定义分配器
ros2 run design_lab rt_alloc_probe --case ipc          # 默认分配器 + 进程内通信，每次 make_unique
ros2 run design_lab rt_alloc_probe --case custom-ipc   # 自定义分配器 + 进程内通信
ros2 run design_lab rt_alloc_probe --case custom --trace 2>&1 | c++filt | less   # 看每次系统 new 的调用栈
ros2 run design_lab rt_alloc_probe --case atomics      # 不跑 ROS：几种 std::atomic 是否无锁
```

| 问题 | 你的预测 | 实测 |
|---|---|---|
| `default`：每条消息 publish + spin 一共调用几次系统 `new`？ | | |
| `custom`：换成自定义分配器后，系统 `new` 降到 0 了吗？ | | |
| `custom --trace`：剩下的系统 `new` 发生在哪一层？ | | |
| `custom-ipc`：进程内通信路径上还有系统 `new` 吗？在哪？ | | |
| `std::atomic<double>` 无锁吗？3 个 `double` 的结构体呢？ | | |

<details>
<summary>参考结果（2026-11-23，rclcpp 33.2.0，Fast DDS 3.6.2）</summary>

```text
default     publish(): 系统 new 1.00 次/条, 自定义 0.00 次/条 | spin_some(): 系统 new 3.00 次/条, 自定义 0.00 次/条 | 收到 1000/1000
custom      publish(): 系统 new 1.00 次/条, 自定义 0.00 次/条 | spin_some(): 系统 new 2.00 次/条, 自定义 1.00 次/条 | 收到 1000/1000
ipc         publish(): 系统 new 3.00 次/条, 自定义 0.00 次/条 | spin_some(): 系统 new 1.00 次/条, 自定义 0.00 次/条 | 收到 1000/1000
custom-ipc  publish(): 系统 new 2.00 次/条, 自定义 1.00 次/条 | spin_some(): 系统 new 1.00 次/条, 自定义 0.00 次/条 | 收到 1000/1000
```

`custom --trace` 的调用栈（节选，已用 `c++filt` 还原函数名）：

```text
======== publish() ========
---- 系统 new(72 字节) 的调用栈 ----
libfastdds.so.3.6(eprosima::fastdds::dds::detail::DataReaderHistory::add_to_instance(...))
...
libfastdds.so.3.6(eprosima::fastdds::rtps::StatefulWriter::intraprocess_delivery(...))
...
librmw_fastrtps_cpp.so(rmw_fastrtps_shared_cpp::__rmw_publish(...))
======== spin_some() ========
---- 系统 new(8 字节) 的调用栈 ----
librmw_fastrtps_cpp.so
librcl.so(rcl_take)
librclcpp.so(rclcpp::SubscriptionBase::take_type_erased(void*, rclcpp::MessageInfo&))
---- 系统 new(168 字节) 的调用栈 ----
librmw_fastrtps_cpp.so
librcl.so(rcl_take)
```

`custom-ipc --trace`：publish 时的 2 次系统 `new` 在 `IntraProcessManager::do_intra_process_publish` 里，spin 时的 1 次在 `SubscriptionIntraProcess` 里。

```text
std::atomic<bool>::is_always_lock_free      = 1
std::atomic<double>::is_always_lock_free    = 1
std::atomic<int64_t>::is_always_lock_free   = 1
std::atomic<Pose3d{3 x double}>.is_lock_free() = 0（24 字节）
std::atomic<Pose2D *>.is_lock_free()       = 1
```

结论：自定义分配器只接管了消息本身那一次分配，剩下的都在 rmw、Fast DDS 和 IntraProcessManager 里面，用户改不了。设计文档"rmw 全面实现分配器接口"的目标还没达到。

</details>

## 2. 进程内通信与借用消息

```bash
ros2 run design_lab ipc_design_probe --case per-sub-buffer
ros2 run design_lab ipc_design_probe --case keep-all
ros2 run design_lab ipc_design_probe --case transient-local
ros2 run design_lab ipc_design_probe --case reliability
ros2 run design_lab ipc_design_probe --case buffer-type
ros2 run design_lab ipc_design_probe --case double-queue
ros2 run design_lab ipc_design_probe --case topics
ros2 run design_lab ipc_design_probe --case mixed-transient
ros2 run design_lab ipc_design_probe --case loan
ROS_DISABLE_LOANED_MESSAGES=0 ros2 run design_lab ipc_design_probe --case loan
ROS_DISABLE_LOANED_MESSAGES=1 ros2 run design_lab ipc_design_probe --case loan
# 对照：加 --no-ipc 关掉进程内通信（对 per-sub-buffer、transient-local、reliability 有效）
```

| 问题 | 你的预测 | 实测 |
|---|---|---|
| 发布者深度 10，先发 0..7 再 spin：深度 2 和深度 5 的订阅者各收到什么？ | | |
| `KeepAll` 的订阅者、发布者能开进程内通信吗？ | | |
| `transient_local` 深度 3，先发 0..4：晚到的 `transient_local` 订阅者收到什么？`volatile` 的呢？ | | |
| best_effort 发布 → reliable 订阅，在进程内通不通？ | | |
| 两个 `UniquePtr` 订阅者，缓冲区类型 `SharedPtr` 时，谁拿到原件？ | | |
| 订阅者深度 3，同时收进程内 5 条、进程间 5 条，最多攒几条？ | | |
| 图里还有 `/xxx/_intra` 话题吗？ | | |
| 只有进程内订阅者时发的 `transient_local` 消息，后来的进程间订阅者能拿到吗？ | | |
| 默认设置下，`Float64` 的发布者、订阅者能借用吗？ | | |
| 开着进程内通信发借用消息，同进程订阅者收到的是同一块内存吗？ | | |

<details>
<summary>参考结果</summary>

```text
ipc=1 发布者深度 10，先发 0..7 再 spin：深度 2 的订阅者收到 [6,7]，深度 5 的收到 [3,4,5,6,7]
KeepAll 订阅者 + 进程内通信：抛异常 intraprocess communication on topic 'ka' allowed only with keep last history qos policy
KeepAll 发布者 + 进程内通信：抛异常 intraprocess communication on topic 'ka2' allowed only with keep last history qos policy
ipc=1 发布者 transient_local 深度 3，先发 0..4：晚到的 transient_local 订阅者收到 [2,3,4]，volatile 订阅者收到 []
[WARN] [ipc_probe]: New publisher discovered on topic '/r1', offering incompatible QoS. No messages will be sent to it. Last incompatible policy: RELIABILITY_QOS_POLICY
ipc=1 best_effort 发布 -> reliable 订阅：收到 0/5；reliable 发布 -> best_effort 订阅：收到 5/5
缓冲区 CallbackDefault 发布 0x56383a1283d0，订阅者 1 收到 0x56383a1289c0（副本），订阅者 2 收到 0x56383a1283d0（原件）
缓冲区 SharedPtr       发布 0x56383a1ef110，订阅者 1 收到 0x56383a1ef740（副本），订阅者 2 收到 0x56383a1ef740（副本）
订阅者深度 3；进程内发 100..104、进程间发 200..204 后才 spin：收到 6 条 [202,102,203,103,204,104]
开了进程内通信后，图里的话题：
  /parameter_events
  /rosout
  /scan_like
进程内订阅者收到 [0,1,2,3,4]；发完之后才出现的进程间 transient_local 订阅者收到 [2,3,4]
```

`--no-ipc` 时，`per-sub-buffer`、`transient-local`、`reliability` 三项的结果和上面完全一样：新的进程内通信在这几项上的行为和走 DDS 一致。

`SharedPtr` 缓冲区那一行，两个副本的地址相同，是因为第一份副本用完释放后，同一块内存又分给了第二份 💡。

借用消息：

```text
# 默认
[INFO] [rclcpp]: Currently used middleware can't loan messages. Local allocator will be used.
Float64     发布者 can_loan_messages=1，订阅者 can_loan_messages=0
FixedBlock  发布者 can_loan_messages=1，订阅者 can_loan_messages=0
String      发布者 can_loan_messages=0，订阅者 can_loan_messages=0
ipc=0 发布借用的 FixedBlock（地址 0x55a7375d2600）：同进程订阅者收到 1 条，地址 0x55a7376d6fd0
ipc=1 发布借用的 FixedBlock（地址 0x55a7376dc010）：同进程订阅者收到 1 条，地址 0x55a7377dc020

# ROS_DISABLE_LOANED_MESSAGES=0
Float64     发布者 can_loan_messages=1，订阅者 can_loan_messages=1
FixedBlock  发布者 can_loan_messages=1，订阅者 can_loan_messages=1
String      发布者 can_loan_messages=0，订阅者 can_loan_messages=0
ipc=0 发布借用的 FixedBlock（地址 0x5616b89308f0）：同进程订阅者收到 1 条，地址 0x5616b89308f0   ← 同一块内存
ipc=1 发布借用的 FixedBlock（地址 0x5616b8a39e80）：同进程订阅者收到 1 条，地址 0x5616b8b39e90

# ROS_DISABLE_LOANED_MESSAGES=1：所有 can_loan_messages 都是 0，地址都不同
```

原因在 rcl 的头文件里：发布者默认 `disable_loaned_message = false`（`ROS_DISABLE_LOANED_MESSAGES=1` 才关），订阅者默认 `disable_loaned_message = true`（`ROS_DISABLE_LOANED_MESSAGES=0` 才开）。而 `publish(LoanedMessage)` 在能借时只调 `rcl_publish_loaned_message`，不走进程内通信。

</details>

## 3. rmw、序列化与网络流

### 3a. rmw 句柄与原生句柄

```bash
ros2 run design_lab rmw_probe --case identity
ros2 run design_lab rmw_probe --case native
```

| 问题 | 你的预测 | 实测 |
|---|---|---|
| 序列化格式叫什么？ | | |
| Fast DDS 的 `DataWriter` 默认是同步发布还是异步发布？ | | |

<details>
<summary>参考结果</summary>

```text
rmw_get_implementation_identifier() = rmw_fastrtps_cpp
rmw_get_serialization_format()      = cdr
rmw_publisher_t::implementation_identifier = rmw_fastrtps_cpp
rmw_publisher_t::topic_name = /rmw_probe，data 指针（不透明）= 0x56486719cad0
原生 DataWriter：类型名 std_msgs::msg::dds_::Int32_
  history.depth = 7（ROS 里给的 7）
  publish_mode  = SYNCHRONOUS（ROS QoS 里没有这一项）
  data_sharing  = OFF（ROS QoS 里没有这一项）
  resource_limits.max_samples = -1
（String、FixedBlock 的 DataWriter 结果相同，类型名分别是 std_msgs::msg::dds_::String_、study_interfaces::msg::dds_::FixedBlock_）
```

</details>

### 3b. 拷贝和序列化哪个贵

```bash
ros2 run design_lab rmw_probe --case serialize
```

| 问题 | 你的预测 | 实测 |
|---|---|---|
| `Int32` 序列化后几个字节？`String("hello")` 呢？ | | |
| 1 MB 的定长 `float` 数组（`FixedBlock`）：序列化比拷贝慢几倍？ | | |
| 1 MB 的 `PoseArray`（18724 个位姿）：慢几倍？ | | |

<details>
<summary>参考结果（耗时随机器变化，看倍数）</summary>

```text
CDR 字节数：Int32 = 8，String("hello") = 14，空 String = 9
FixedBlock（1 MB 定长）   1048588 字节 | 拷贝     45.7 μs | 序列化     53.7 μs | 反序列化     54.4 μs | 序列化/拷贝 = 1.2 倍
PoseArray（18724 个位姿）   1048572 字节 | 拷贝     50.1 μs | 序列化    797.0 μs | 反序列化    785.8 μs | 序列化/拷贝 = 15.9 倍
```

设计文档说"序列化比拷贝慢至少两个数量级"。在今天的 Fast DDS 上，平坦数组的序列化几乎就是一次 `memcpy`；字段多而碎的结构慢一个数量级左右。

</details>

### 3c. 运行时才知道类型

```bash
ros2 run design_lab rmw_probe --case generic
```

<details>
<summary>参考结果</summary>

```text
通用发布者发出 31 字节；类型化订阅者收到 "from generic publisher"；通用订阅者收到 31 字节
不存在的类型名：抛异常 Something went wrong loading the typesupport library for message type std_msgs/NoSuchType. Library could not be found.
```

抛异常之前还会打印一段 `rcutils_set_error_state()` 的"error state is being overwritten"警告，可以忽略。

</details>

### 3d. 消息定义能不能写可选字段、字典

```bash
python3 - <<'EOF'
from rosidl_adapter.parser import parse_message_string
for txt in ["int32 x 5", "@optional int32 x", "map<string,int32> m"]:
    try:
        m = parse_message_string("pkg", "M", txt)
        print(repr(txt), "-> OK:", [(str(f.type), f.name, f.default_value, f.annotations) for f in m.fields])
    except Exception as e:
        print(repr(txt), "-> 失败:", str(e)[:120])
EOF
```

解析器接受的写法，生成的 C++ 是什么样？建一个只有一个 `.msg` 的临时包就能看到。例如 `msg/Opt.msg` 写 `int32 a`、`@optional int32 b`、`@optional string c` 三行，`colcon build` 之后打开 `build/<包名>/rosidl_generator_cpp/<包名>/msg/detail/opt__struct.hpp`，看 `b`、`c` 的类型。

<details>
<summary>参考结果</summary>

```text
'int32 x 5' -> OK: [('int32', 'x', 5, {'comment': []})]
'@optional int32 x' -> OK: [('int32', 'x', None, {'optional': True, 'comment': []})]
'map<string,int32> m' -> 失败: 'map<string,int32>' is an invalid message name. It should have the pattern '^[A-Z][A-Za-z0-9]*$'
```

（解析失败前 stderr 上还有一行 `Error processing 'map<string,int32> m' of 'pkg/M': ...`。）

生成的 `opt__struct.hpp` 里，`b` 的类型是 `int32_t`，`c` 是 `std::basic_string<...>`，和普通字段没有区别；生成的 `.idl` 里保留了 `@optional`。所以 `@optional` 写了不报错，但在 C++ 里没有效果。

</details>

### 3e. 唯一网络流

```bash
ros2 run design_lab rmw_probe --case flows
```

| 问题 | 你的预测 | 实测 |
|---|---|---|
| Fast DDS 上，发布者用 `STRICTLY_REQUIRED` 能创建吗？ | | |
| 订阅者用 `STRICTLY_REQUIRED` 时，端点和默认的有什么不同？ | | |
| 流标签、DSCP 被设了吗？ | | |

<details>
<summary>参考结果（地址是容器的 IP）</summary>

```text
发布者 NOT_REQUIRED         端点：
      UDP/IPv4 0.0.0.0:40240 flow_label=0 dscp=0
      UDP/IPv4 192.0.2.2:59227 flow_label=0 dscp=0
订阅者 NOT_REQUIRED         端点：
      UDP/IPv4 :7411 flow_label=0 dscp=0
      UDP/IPv4 192.0.2.2:7411 flow_label=0 dscp=0
发布者 STRICTLY_REQUIRED    创建失败：could not create publisher: Unique network flow endpoints not supported on publishers, at ./src/publisher.cpp:98, at ./src/rcl/publisher.c:126
订阅者 STRICTLY_REQUIRED    端点：
      UDP/IPv4 :7001 flow_label=0 dscp=0
      UDP/IPv4 192.0.2.2:7001 flow_label=0 dscp=0
发布者 OPTIONALLY_REQUIRED  端点：（和 NOT_REQUIRED 相同）
订阅者 OPTIONALLY_REQUIRED  端点：
      UDP/IPv4 :7002 flow_label=0 dscp=0
      UDP/IPv4 192.0.2.2:7002 flow_label=0 dscp=0
发布者 SYSTEM_DEFAULT       端点：（和 NOT_REQUIRED 相同）
订阅者 SYSTEM_DEFAULT       端点：（和 NOT_REQUIRED 相同，7411）
```

</details>

## 挑战（选做）

1. 从源码编译 [ros2/realtime_support](https://github.com/ros2/realtime_support) 的 `tlsf_cpp`（依赖 [ros2/tlsf](https://github.com/ros2/tlsf)），把 `rt_alloc_probe` 里的 `CountingAllocator` 换成 `tlsf_heap_allocator`。系统 `new` 的次数会变吗？为什么？
2. 给 `rt_alloc_probe` 加一个 `--case sensor`，发布 1 MB 的 `FixedBlock`。系统 `new` 的次数和 `Int32` 一样吗？每条消息分配的字节数呢？
3. 把 `ipc_design_probe --case loan` 改成两个进程（一个只发、一个只收），在 `ROS_DISABLE_LOANED_MESSAGES=0` 下看订阅端收到的消息是否来自共享内存。提示：Fast DDS 的 data sharing 要满足类型有界、QoS 合适等条件，可以用 `rmw_probe --case native` 的办法读 `data_sharing`。

## 做完之后

回到三篇设计笔记的对照表，把你预测错的那几行再读一遍；然后合上页面，做笔记末尾的自测题。
