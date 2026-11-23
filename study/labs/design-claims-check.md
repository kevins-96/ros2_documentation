# 练习：设计文档的说法在 Rolling 里还成立吗？

> 适用：ROS 2 **Rolling**（默认 RMW `rmw_fastrtps_cpp`）。对应设计笔记：[ROS 图怎么映射到 DDS](../design/graph-to-dds-mapping.md)、[QoS 的设计](../design/qos-design.md)、[服务与动作的设计](../design/service-action-design.md)、[接口定义的设计](../design/interface-design.md)
> 代码：[design_lab/](design_lab/)（依赖 [study_interfaces](study_interfaces/)，本批给它加了 `FixedGrid.msg`）
> 用时：约 40 分钟，可以分几次做

设计文档写的是**设计意图**，很多写于实现之前。这个练习把其中能验证的说法逐条拿到 Rolling 上跑一遍。每部分都先填预测，再运行，再展开参考结果。**预测错了的地方，就是你对设计理解有偏差的地方**，回到对应的设计笔记再读一遍。

## 0. 编译

```bash
ln -s <你的仓库路径>/study/labs/* ~/study_ws/src/
cd ~/study_ws && source /opt/ros/rolling/setup.bash
colcon build --packages-up-to design_lab && source install/setup.bash
```

第 2 部分要抓包，需要 `tcpdump`（`sudo apt install tcpdump`）和 root 权限。用官方镜像最省事：

```bash
docker run -it --rm -v <你的仓库路径>/study/labs:/ws/src/labs ros:rolling-ros-base
apt-get update && apt-get install -y tcpdump binutils   # 容器里
```

## 1. 名字规则与 ignore_local_publications

```bash
ros2 run design_lab graph_mapping_probe
ros2 run design_lab graph_mapping_probe --separate-context   # 只看最后一行
```

程序在一个进程里建两个节点 `/robot1/planner`、`/robot1/controller`，打印各种名字的展开结果，再测 `ignore_local_publications`。

| 问题 | 你的预测 | 实测 |
|---|---|---|
| `"~/debug"`、`"{node}/status"`、`"{ns}/fleet_status"` 分别展开成什么？ | | |
| `"foo__bar"`（连续下划线）会被拒绝吗？设计文档说不允许 | | |
| `/robot1/` + 239 个 `a` 和 + 240 个 `a`，哪个被拒绝？ | | |
| `avoid_ros_namespace_conventions=true` 的 `"raw_dds_topic"`，`get_topic_name()` 是什么？ | | |
| `ignore_local_publications=true` 的订阅：同节点发 10 条收到几条？同进程另一个节点发 10 条呢？ | | |
| 加 `--separate-context` 后，"另一个节点"的 10 条收到几条？ | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

2026-11-23 在 `ros:rolling-ros-base`（rclcpp 33.2.0、rcl 10.6.0、`rmw_fastrtps_cpp` 9.6.1）里实测：

```text
[1] 名字展开（节点 /robot1/planner）
  "plan"                       -> /robot1/plan
  "/global_plan"               -> /global_plan
  "~/debug"                    -> /robot1/planner/debug
  "{node}/status"              -> /robot1/planner/status
  "{ns}/fleet_status"          -> /robot1/fleet_status
  "_internal/heartbeat"        -> /robot1/_internal/heartbeat
  "raw_dds_topic"              -> /robot1/raw_dds_topic （avoid_ros_namespace_conventions=true）
  服务 "compute"               -> /robot1/compute
[2] 非法名字
  "foo__bar"                   -> /robot1/foo__bar
  "~foo"                       -> 拒绝: ... tilde '~' that is not followed by a forward slash '/'
  "123abc"                     -> 拒绝: ... topic name token must not start with a number
  "foo/"                       -> 拒绝: ... must not end with a forward slash
  "foo//bar"                   -> 拒绝: ... must not contain repeated '/'
  "{bad/sub}"                  -> 拒绝: ... substitution name must not contain characters other than alphanumerics or '_'
  "aaaa...（239 个）"           -> /robot1/aaaa...
  "aaaa...（240 个）"           -> 拒绝: ... topic length should not exceed '255U - 8U'
[3] ignore_local_publications=true 的订阅，谁的消息会被忽略？
  同节点发布 10 条，收到 0；同进程另一个节点发布 10 条，收到 0（对照组普通订阅共收到 19 条）
# --separate-context：
  同节点发布 10 条，收到 0；同进程另一个节点发布 10 条，收到 10（对照组普通订阅共收到 19 条）
```

解读：
- `{node}`、`{ns}` 是 rcl 定义的替换（还有 `{namespace}`）；设计文档只规定了语法，没规定有哪些替换。
- `foo__bar` 被接受：设计文档的"不能有连续下划线"在 Rolling 里没有执行。
- 长度上限是完整 ROS 名字 247 个字符（255 减去留给 `rt/` 这类前缀的 8 个）。
- `avoid_ros_namespace_conventions` 不影响 ROS 名字本身，影响的是 DDS 名字，见第 2 部分。
- `ignore_local_publications` 忽略的是**同一个 DDS 参与者**发的消息。默认一个 context 一个参与者，所以同进程的兄弟节点也被忽略；分到两个 context 后才只忽略自己。对照组差 1 条是发现刚完成时的边界情况，不影响结论。

</details>

## 2. 从网线上看 DDS 名字和参与者

终端 A（先启动抓包，6 秒后自动停止并打印）：

```bash
ros2 run design_lab capture_dds_names.sh 6
```

终端 B（1 秒内启动）：

```bash
ros2 run design_lab graph_mapping_probe --hold 4 &
sleep 2 && ros2 run design_lab participant_map
```

| 问题 | 你的预测 | 实测 |
|---|---|---|
| 话题 `/robot1/plan`、服务 `/robot1/compute` 在 DDS 里叫什么？ | | |
| 动作 `move_to` 有没有用设计文档表里的 `ra/` 前缀？参数服务有没有用 `rp/`？ | | |
| `raw_dds_topic` 在 DDS 里叫什么？ | | |
| `std_msgs/msg/String` 在 DDS 里的类型名是什么？ | | |
| `participant_map` 会列出几个参与者？`planner` 和 `controller` 在不在同一个里？ | | |

<details>
<summary>参考结果</summary>

`capture_dds_names.sh` 的输出（节选）：

```text
== DDS 话题名 ==
ros_discovery_info
rq/robot1/computeRequest
rq/robot1/move_to/_action/cancel_goalRequest
rq/robot1/move_to/_action/get_resultRequest
rq/robot1/move_to/_action/send_goalRequest
rq/robot1/planner/get_parametersRequest
rr/robot1/computeReply
rr/robot1/move_to/_action/send_goalReply
rt/global_plan
rt/parameter_events
rt/robot1/_internal/heartbeat
rt/robot1/move_to/_action/feedback
rt/robot1/move_to/_action/status
rt/robot1/plan
rt/robot1/planner/debug
rt/rosout
/robot1/raw_dds_topic
== DDS 类型名 ==
std_msgs::msg::dds_::String_
study_interfaces::srv::dds_::ComputePath_Request_
study_interfaces::action::dds_::MoveTo_SendGoal_Request_
rmw_dds_common::msg::dds_::ParticipantEntitiesInfo_
...
```

`participant_map` 的输出（GUID 前缀每次不同）：

```text
共 2 个参与者
  参与者 010feb7d.d80786d7.00000000： /robot1/planner /robot1/controller
  参与者 010feb7d.ed0791cb.00000000： /participant_map
```

加 `--separate-context` 再跑一次，变成 3 个参与者，`planner` 和 `controller` 的前缀只差最后 4 个字节里的参与者编号。

解读：
- 话题 `rt/`；服务是一对话题 `rq/<名字>Request`、`rr/<名字>Reply`。设计文档表里的 `rs/`、`rp/`、`ra/` 都没用上：参数服务就是普通服务，动作是 `rt/` 话题加 `rq/rr` 服务，靠 `_action` 识别。
- `avoid_ros_namespace_conventions` 只去掉 `rt` 前缀，命名空间和开头的 `/` 都保留，DDS 名字是 `/robot1/raw_dds_topic`，不是设计文档表里的 `raw_dds_topic`。
- `ros_discovery_info` 是 rmw 内部话题（参与者 → 节点 → 读写端），rclcpp 订阅不到它（名字前总会被加上 `/`），所以 `participant_map` 改用公开的 `get_publishers_info_by_topic("/rosout")`，按 GID 前 12 字节（RTPS GUID 前缀）分组。
- 一个 context 一个参与者，和设计文档一致。

</details>

## 3. QoS：Deadline、事件合并、Liveliness、Lifespan

```bash
ros2 run design_lab qos_semantics_lab --case deadline-rmw
ros2 run design_lab qos_semantics_lab --case coalesce
ros2 run design_lab qos_semantics_lab --case liveliness
ros2 run design_lab qos_semantics_lab --case liveliness --same-participant
ros2 run design_lab qos_semantics_lab --case lifespan
```

| `--case` | 设置 | 问题 | 你的预测 | 实测 |
|---|---|---|---|---|
| `deadline-rmw` | 上游 10 Hz 准时发，两端 deadline 200 ms；订阅回调每条 400 ms | 会报 deadline missed 吗？回调拿到的消息最老多老？ | | |
| `coalesce` | 上游 1.0 s 停发，deadline 100 ms；订阅端执行器在 1.2–2.2 s 被卡住 | 卡住期间违约约几次？恢复后回调几次、`total_count_change` 多少？ | | |
| `liveliness` | 租约 300 ms。MANUAL_BY_TOPIC 每 1 s 发一条；AUTOMATIC 只在 0.5 s 发一条，2.5 s 被销毁；看门狗在另一个参与者 | 两者的 alive/not_alive 怎么变化？ | | |
| `liveliness --same-participant` | 同上，但看门狗和发布者在同一个参与者 | AUTOMATIC 有什么不同？ | | |
| `lifespan` | TransientLocal + Lifespan 500 ms，发 5 条 | 0.2 s 后、1.0 s 后加入的订阅者各收到几条？ | | |

<details>
<summary>参考结果</summary>

```text
[deadline-rmw] 4 s 内：回调处理 10 条，消息到回调时最老 1001 ms；订阅端 deadline missed 0 次，发布端 0 次

[coalesce] 上游 t=1.0 s 停发，订阅端执行器在 t≈1.2~2.2 s 被卡住，deadline=100 ms
  t=1.08 s  回调一次: total_count=1 total_count_change=1
  t=1.18 s  回调一次: total_count=2 total_count_change=1
  t=2.23 s  回调一次: total_count=12 total_count_change=10
  t=2.27 s  回调一次: total_count=13 total_count_change=1
  t=2.37 s  回调一次: total_count=14 total_count_change=1
  ...

[liveliness] 租约 300 ms；MANUAL_BY_TOPIC 每 1 s 发一条；AUTOMATIC 只在 0.5 s 发一条，2.5 s 被销毁
  t=0.23 s  AUTOMATIC alive=1 not_alive=0
  t=1.03 s  MANUAL    alive=1 not_alive=0
  t=1.33 s  MANUAL    alive=0 not_alive=1
  t=2.03 s  MANUAL    alive=1 not_alive=0
  t=2.33 s  MANUAL    alive=0 not_alive=1
  t=3.03 s  AUTOMATIC alive=0 not_alive=0
  t=3.03 s  MANUAL    alive=1 not_alive=0
  t=3.33 s  MANUAL    alive=0 not_alive=1

[liveliness 同一参与者] ...
  t=0.53 s  AUTOMATIC alive=1 not_alive=0
  t=0.83 s  AUTOMATIC alive=0 not_alive=1
  （MANUAL 同上）

[lifespan] 发布 5 条（TransientLocal，lifespan 500 ms）：t≈0.2 s 加入的订阅者收到 5 条，t≈1.0 s 加入的收到 0 条
```

解读：
- **Deadline 只量到 rmw 层**（设计如此）：消息按时到了 rmw 就不算违约，哪怕你的回调 1 s 后才处理它。控制器里要自己检查消息年龄。
- **事件合并**（设计如此）：执行器被卡住期间的 10 次违约合成一次回调，`total_count_change=10`。计数用 `total_count`/`total_count_change`，别数回调次数。
- **MANUAL_BY_TOPIC**：每次发布后活 300 ms，然后 not_alive，符合"只有发布或显式 assert 才算活着"。
- **AUTOMATIC，看门狗在另一个参与者**：匹配上就 alive（一条都没发），销毁后 alive 变 0。销毁发生在 2.5 s，事件 3.03 s 才被处理，恰好是下一条 MANUAL 消息到达的时刻，说明"发布者消失"的事件不一定立刻唤醒执行器（只观察到这个现象，原因未深究）。
- **AUTOMATIC，同一参与者**：直到第一次发布才 alive，300 ms 后就报 not_alive，尽管进程好好的。看门狗别和被看的节点放在同一个组件容器里。
- **Lifespan**：过期样本从发布者的历史里移除，后来者拿不到旧数据。

</details>

## 4. 动作：隐藏名字、结果缓存、状态话题

```bash
ros2 run design_lab action_result_cache
ros2 run design_lab action_result_cache --result-timeout 60
```

程序建一个动作服务端（0.2 s 完成目标）、一个正常客户端、一个"旁观者"：旁观者直接调用隐藏的 `get_result` 服务，在目标完成后 3 s 和 12 s 各取一次；还在完成后、以及结果过期后各新建一个状态订阅。

| 问题 | 你的预测 | 实测 |
|---|---|---|
| 一个动作展开成哪几个话题和服务？名字有什么共同点？ | | |
| 默认 `result_timeout` 是多少？完成后 3 s、12 s 取结果分别得到什么？ | | |
| 完成后才建的状态订阅能拿到状态吗？结果过期后再建的呢？ | | |
| `--result-timeout 60` 时 12 s 取结果得到什么？ | | |

<details>
<summary>参考结果</summary>

```text
result_timeout = 10.0 s
[1] 一个动作在图里展开成：
    话题 /move_to/_action/feedback            study_interfaces/action/MoveTo_FeedbackMessage
    话题 /move_to/_action/status              action_msgs/msg/GoalStatusArray
    服务 /move_to/_action/cancel_goal         action_msgs/srv/CancelGoal
    服务 /move_to/_action/get_result          study_interfaces/action/MoveTo_GetResult
    服务 /move_to/_action/send_goal           study_interfaces/action/MoveTo_SendGoal
[2] 目标完成：SUCCEEDED
[3] 后加入的状态订阅收到 1 个目标的状态，第一个是 SUCCEEDED
[2] 完成后  3 s，旁观者调用 get_result：status=SUCCEEDED reached=true
[2] 完成后 12 s，旁观者调用 get_result：status=UNKNOWN reached=false
[3] 结果过期后新建的状态订阅收到 1 个目标的状态，第一个是 SUCCEEDED
# --result-timeout 60：
[2] 完成后 12 s，旁观者调用 get_result：status=SUCCEEDED reached=true
```

解读：
- 3 个服务 + 2 个话题，都在 `<动作名>/_action/` 下；`_action` 以下划线开头，所以 `ros2 topic list` 默认不显示。和设计文档一致。
- 结果缓存 10 s（rcl_action 默认值），过期后 `get_result` 返回 UNKNOWN。需要更久就在 `create_server` 时传 `rcl_action_server_options_t`。
- 状态话题是 TransientLocal、深度 1，后加入也能拿到。但结果过期时服务端没有重新发布状态，所以过期后新建的订阅仍看到 SUCCEEDED，和 `get_result` 的 UNKNOWN 不一致。
- 目标 ID 是客户端生成的 UUID，旁观者拿着它就能查结果。

</details>

## 5. 生成的消息结构体

```bash
ros2 run design_lab msg_init_bench
```

`FixedGrid` 有一个 `float32[262144]` 定长数组（1 MB）、一个带默认值的 `float64 resolution 0.05` 和一个 `string<=8 tag`。

| 问题 | 你的预测 | 实测 |
|---|---|---|
| `ALL`、`ZERO`、`DEFAULTS_ONLY` 构造后 `resolution` 各是多少？ | | |
| 四种模式构造 1 MB 消息的耗时排序？ | | |
| `PathStatus.waypoints`（`[<=1000]`）`resize(1001)` 会怎样？ | | |
| `tag`（`string<=8`）赋 20 个字符，赋值、序列化、反序列化哪一步报错？ | | |

<details>
<summary>参考结果</summary>

```text
FixedGrid: cells 有 262144 个 float（1.0 MB），resolution 默认值 0.05
ALL:           resolution=0.05 cells[0]=0.0
ZERO:          resolution=0.00 cells[0]=0.0
DEFAULTS_ONLY: resolution=0.05 （cells 未初始化）
每次构造平均耗时（300 次）：
  ALL               23.5 us
  ZERO              47.3 us
  DEFAULTS_ONLY      0.0 us
  SKIP               0.0 us
setter 链：width=512 height=512 resolution=0.10
builder：status=0 length_m=3.5
有界数组 [<=1000] resize(1001)：立刻抛出 Exceeded upper bound
有界字符串 string<=8 赋值 20 个字符：没有报错
  序列化（发布时也走这一步）：没有报错，1048621 字节
  反序列化（订阅端）：没有报错，tag="this_tag_is_too_long"（20 个字符）
```

解读：
- `ZERO` 忽略消息里写的默认值；`DEFAULTS_ONLY` 只填默认值。
- `ZERO` 比 `ALL` 慢一倍：生成的构造函数在 ZERO 分支里把数组清零了两次（看 `fixed_grid__struct.hpp`）。热路径上用 `DEFAULTS_ONLY`，前提是你会写满其他字段。
- 测耗时要用编译器屏障：没有它，编译器发现 300 次清零里前 299 次都被下一次覆盖，会直接删掉，测出来全是 0（这个程序第一版就踩了这个坑）。
- 有界数组在修改时就检查；有界字符串在 Fast DDS 上从赋值到反序列化都没检查。设计文档说"约束只保证在发布时检查"，实际连发布时也没有。

</details>

## 6. 挑战（选做）

1. 把第 1 部分的程序改成用 `rmw_cyclonedds_cpp`（需要安装）再跑一遍 `ignore_local_publications`，结论一样吗？设计文档说当年 Fast DDS 根本没实现这个选项，现在呢？
2. 给第 4 部分的客户端打开 `enable_feedback_msg_optimization`，同时开两个客户端各发一个目标，用 `ros2 topic hz /move_to/_action/feedback` 和客户端里的反馈计数对比，验证"每个客户端只收自己的反馈"。
3. 在 `capture_dds_names.sh` 的抓包里找 `enclave=`，看看 SROS2 的 enclave 名字是怎么随参与者发出去的（第 7 批的[安全卡片](../cards/middleware/sros2-security.md)说加密后它仍是明文）。
