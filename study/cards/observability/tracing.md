---
tags: [ros2, tracing, lttng, ros2_tracing, tracetools, 性能, 调试]
distro: rolling
status: batch-6
aliases: [tracing, 性能追踪, ros2_tracing, tracetools, LTTng, ros2 trace, babeltrace, callback_start, callback_end, tracetools_analysis, 回调时长]
---

# tracing 性能追踪

> 适用：ROS 2 **Rolling**（2026-11-17 在 `ros:rolling-ros-base` 容器里实测，LTTng 用户态追踪 + babeltrace2）
> 来源标记：📄 仓库文档原文 · 🔍 源码或实测核实 · 💡 经验补充（可信，但请自己验证）
> 源文档：[About-introspection-and-analysis.rst](../../../source/Developer-Tools/About-introspection-and-analysis.rst)、[ROS2-Tracing-Trace-and-Analyze.rst](../../../source/Developer-Tools/Introspection-and-analysis/Tracing/ROS2-Tracing-Trace-and-Analyze.rst)、[Building-ROS-2-with-Tracing.rst](../../../source/Developer-Tools/Debugging/Building-ROS-2-with-Tracing.rst)

## 1. 一句话定义

ROS 2 的核心库（rcl、rclcpp、rmw）里埋了 **tracepoint**：创建节点、发布、取消息、回调开始/结束时各记一条带纳秒时间戳的事件。用 LTTng 打开一个追踪会话，程序照常运行，事后把轨迹（trace）拿出来分析"每个回调跑了多久、消息走了多久"。这套机制叫 `ros2_tracing`，tracepoint 在 `tracetools` 包里。📄

## 2. 为什么需要

日志和 `ros2 topic hz` 只能看到"结果"：控制频率掉了、路径来晚了。💡 tracing 回答的是**为什么**：是规划回调本身变慢，还是它在执行器里排队等别的回调，还是消息在中间件里耽搁了。它的开销很低，可以在真车上对整个系统开着跑。

📄 Linux 二进制安装默认带 tracepoint；**没开追踪会话时几乎没有开销**（virtually no impact）。🔍 `ros2 run tracetools status` 在镜像里打印 `Tracing enabled`。

### 一次追踪里有什么

🔍 `fake_planner`（10 Hz 定时器）跑 5 秒，`lttng enable-event -u 'ros2:*'` 得到的事件（节选，按条数排序）：

| 事件 | 条数 | 含义 |
|---|---|---|
| `ros2:rmw_publish` / `rcl_publish` / `rclcpp_publish` | 139 / 109 / 108 | 每层发布各记一次 |
| `ros2:rclcpp_executor_get_next_ready`、`wait_for_work`、`execute` | 98 / 49 / 47 | 执行器每一轮在干什么 |
| `ros2:callback_start` / `callback_end` | 47 / 47 | **回调开始/结束，算时长的主力** |
| `ros2:rclcpp_callback_register` | 10 | 回调地址 → 可读符号名（lambda 也有名字） |
| `ros2:rcl_node_init`、`rcl_timer_init`、`rcl_publisher_init` ... | 各 1–8 | 初始化：建立"哪个回调属于哪个节点、哪个定时器"的映射 |

```text
[16:15:00.665356488] ros2:callback_start: { vtid = 4640, procname = "fake_planner" }, { callback = 0x561DDF88C078, is_intra_process = 0 }
```

💡 初始化事件只在启动时出现一次：**先开追踪会话，再启动节点**，否则只有地址、对不上名字。

### 用 tracing 看"回调超时"

🔍 让定时器回调额外忙等 `work_ms`，定时器周期 100 ms（10 Hz），用 [callback_durations.py](../../labs/observe_lab/scripts/callback_durations.py) 统计：

| `work_ms` | 回调平均时长 | 回调最大时长 | 实际触发周期 | `ros2 topic hz /plan` |
|---|---|---|---|---|
| 0 | 0.180 ms | 0.499 ms | 100.0 ms | 9.999 Hz |
| 30 | 30.197 ms | 30.409 ms | 100.0 ms | 9.999 Hz |
| 120 | 120.191 ms | 120.295 ms | **120.3 ms** | 约 8.3 Hz（间隔 0.120 s） |

回调比周期长时，定时器**不会补发**错过的触发，频率直接掉到 $1/120\,\text{ms} \approx 8.3\,\text{Hz}$。💡 只看 `hz` 你知道频率掉了；看 tracing 你知道是哪个回调、每次多长，还能看到它挤占了谁。

## 3. 最小用法

### 文档的流程（📄，需要 `ros2trace` 和 `tracetools_analysis`）

```bash
sudo apt install babeltrace ros-rolling-ros2trace ros-rolling-tracetools-analysis
ros2 trace --session-name perf-test --list    # 回车开始，再回车结束；轨迹在 ~/.ros/tracing/perf-test
babeltrace ~/.ros/tracing/perf-test | less
# 分析：tracetools_analysis 的 Jupyter notebook（callback_duration.ipynb）画回调时长
```

### 只用 LTTng 命令（🔍 本卡片实测用的方式）

镜像里没有 `ros2trace`，但有 `lttng` 和 `tracetools`，直接用 LTTng 也一样：

```bash
lttng create plan_trace --output=/tmp/traces/plan_trace
lttng enable-event -u 'ros2:*'               # 只开 ROS 2 用户态事件
lttng add-context -u -t vtid -t procname     # 每条事件带线程号和进程名（多线程执行器必需）
lttng start
ros2 run observe_lab fake_planner --ros-args -p work_ms:=30.0 -p rate_hz:=10.0   # Ctrl-C 结束
lttng stop && lttng destroy                  # destroy 只销毁会话，不删轨迹文件
babeltrace2 /tmp/traces/plan_trace | head    # 文本查看
python3 callback_durations.py /tmp/traces/plan_trace   # 统计回调时长（bt2 Python 绑定）
```

统计脚本的核心（完整代码：[callback_durations.py](../../labs/observe_lab/scripts/callback_durations.py)）：

```python
for msg in bt2.TraceCollectionMessageIterator(trace_dir):
    if type(msg) is not bt2._EventMessageConst:
        continue
    ev, t = msg.event, msg.default_clock_snapshot.ns_from_origin
    if ev.name == 'ros2:rclcpp_callback_register':
        names[int(ev['callback'])] = str(ev['symbol'])
    elif ev.name == 'ros2:callback_start':
        open_start[(int(ev['vtid']), int(ev['callback']))] = t
    elif ev.name == 'ros2:callback_end':
        key = (int(ev['vtid']), int(ev['callback']))
        durations[key[1]].append(t - open_start.pop(key))   # 同一线程内成对
```

🔍 输出：`52 calls  mean  30.197 ms  max  30.409 ms  period  100.0 ms  FakePlanner::FakePlanner()::{lambda()#1}`。

### 去掉 tracepoint（📄）

| 目的 | 做法 |
|---|---|
| 去掉 tracepoint（保留函数调用） | 重新编译 `tracetools`：`-DTRACETOOLS_TRACEPOINTS_EXCLUDED=ON`（二进制安装要把 `ros2_tracing` 克隆进工作空间再编） |
| 连函数调用一起去掉 | 从源码编译整个 ROS：`-DTRACETOOLS_DISABLED=ON` |
| 确认结果 | `ros2 run tracetools status` |

💡 一般不需要去掉：不开会话时开销几乎为零。只有对体积或认证有要求的场合才考虑。

## 4. 常见坑

1. **先启动节点、后开追踪，就对不上名字。** 🔍 `rclcpp_callback_register` 等初始化事件只在构造时发一次；会话开晚了，只剩回调地址。💡 对已经在跑的系统，至少能得到时长分布，名字可以从地址和触发周期推断。
2. **多线程执行器下只按回调地址配对会配错。** 💡 两个线程可能同时在跑同一个回调（可重入回调组），`callback_start`/`callback_end` 要按 `(vtid, callback)` 配对，所以 `lttng add-context -u -t vtid` 不能省。本练习是单线程执行器，没有实测多线程的情况。
3. **回调时长 ≠ 端到端延迟。** 💡 回调 30 ms 只是"处理"时间；消息从发布到回调开始之间还有中间件传输和执行器排队。要看端到端，需要把 `rclcpp_publish` 和订阅端的 `rmw_take`/`callback_start` 按消息对应起来（`tracetools_analysis` 做了这件事）。
4. **`ros2 trace` 和 `tracetools_analysis` 不在 ros-base 镜像里。** 🔍 镜像只有 `tracetools` 和 `lttng`；这次容器里 ROS apt 源不可用，没能装上，所以本卡片用 `lttng` + `babeltrace2` + Python `bt2` 代替。📄 正常环境按文档 `apt install ros-<distro>-ros2trace ros-<distro>-tracetools-analysis`。
5. **容器里也能追踪用户态事件。** 🔍 不需要内核模块，`lttng create` 会自动拉起 `lttng-sessiond`。📄 内核事件（调度、中断）需要宿主机装 LTTng 内核模块，容器里一般做不了。
6. **定时器回调超时不会"追赶"。** 🔍 120 ms 的回调配 100 ms 的定时器，实际周期变成 120 ms，没有连续触发补齐。💡 控制回路里这意味着**控制频率悄悄下降**；用 tracing 或回调内计时监控"回调时长 / 周期"的比值。

## 5. 与其他概念的关系

- [执行器](../concurrency/executor.md)、[回调组](../concurrency/callback-group.md)：tracing 能直接看到执行器的每一轮和回调排队
- [实时编程](../concurrency/realtime.md)：抖动和最坏情况时长靠 tracing 测
- [命令行工具](cli-tools.md)：`hz` 看现象，tracing 找原因
- [调试与回溯](gdb-debugging.md)：gdb 看"崩在哪"，tracing 看"慢在哪"
- 练习：[崩溃回溯与回调追踪](../../labs/crash-and-trace.md)
- 上一级：[总览脑图](../../map/overview.md)

## 6. 自测题（先自己答，再看[答案](../../answers/tracing-answers.md)）

1. 局部控制器设定 20 Hz，`ros2 topic hz /cmd_vel` 显示只有 14 Hz 左右。你打算用 tracing 排查。说出要开哪些事件和上下文、什么时候开会话、看哪几个指标来区分"控制回调本身太慢"和"控制回调在执行器里排队"。
2. 本卡片的表格里，`work_ms=120` 时平均周期是 120.3 ms 而不是 200 ms（"每隔一次错过一个周期"）。根据这个结果说明 rclcpp 定时器错过触发后的行为，并推测 `work_ms=250` 时的实际频率。
