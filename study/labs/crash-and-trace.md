# 练习：崩溃回溯与回调追踪

> 适用：ROS 2 **Rolling**。对应卡片：[调试与回溯](../cards/observability/gdb-debugging.md)、[tracing 性能追踪](../cards/observability/tracing.md)
> 代码：[fake_planner.cpp](observe_lab/src/fake_planner.cpp)（`count_waypoints`、`validate_plan`、`busy_wait`）、[callback_durations.py](observe_lab/scripts/callback_durations.py)
> 用时：约 40 分钟。编译见[命令行与服务内省](cli-and-introspection.md)第 0 部分

需要额外安装：

```bash
sudo apt install gdb lttng-tools babeltrace2 python3-bt2
ros2 run tracetools status        # 应打印 Tracing enabled
```

## 1. 不带 gdb 的崩溃

```bash
ros2 run observe_lab fake_planner --ros-args -p crash_on_replan:=true; echo "rc=$?"
# 另一个终端
ros2 service call /replan std_srvs/srv/Trigger
```

预测：终端里会看到什么？退出码是多少？`ros2 service call` 那边会怎样？

<details>
<summary>参考结果（填完预测再展开）</summary>

2026-11-17 在 `ros:rolling-ros-base` 容器里实测（gdb 17.1、GCC 15）：

```text
[ros2run]: Segmentation fault
rc=245
```

245 = 256 − 11，即被 SIGSEGV（11 号信号）杀死。`ros2 service call` 打印 `requester: making request: std_srvs.srv.Trigger_Request()` 之后就一直等，服务端已经没了，响应永远不会来。只凭这些，你不知道崩在哪一行。

</details>

## 2. 四种构建类型下的回溯

分别用四种构建类型编译到不同目录，避免互相覆盖：

```bash
cd ~/study_ws
for bt in None Debug Release RelWithDebInfo; do
  colcon build --packages-select observe_lab \
    --build-base /tmp/bt/$bt/build --install-base /tmp/bt/$bt/install \
    --cmake-args -DCMAKE_BUILD_TYPE=$bt
done
```

对每种构建，新终端里：

```bash
source /tmp/bt/Debug/install/setup.bash        # 换成对应的构建类型
ros2 run --prefix 'gdb -q -batch -ex run -ex bt -ex "frame 0" -ex "info locals" --args' \
  observe_lab fake_planner --ros-args -p crash_on_replan:=true; echo "rc=$?"
# 另一个终端触发
ros2 service call /replan std_srvs/srv/Trigger
```

| 构建类型 | 预测：第 0 帧是哪个函数？有行号吗？`info locals` 能看到 `plan` 吗？ | 实测 |
|---|---|---|
| `None`（不设） | | |
| `Debug` | | |
| `Release` | | |
| `RelWithDebInfo` | | |

还有一个问题：用 gdb 批处理模式包起来后，`ros2 run` 的退出码是多少？

<details>
<summary>参考结果（填完预测再展开）</summary>

| 构建类型 | 第 0 帧 | 行号 / 变量 |
|---|---|---|
| `None` | `(anonymous namespace)::count_waypoints((anonymous namespace)::Plan const*) ()` | 无行号；`No symbol table info available.` |
| `Debug` | `count_waypoints (plan=0x0) at .../fake_planner.cpp:31`，源码行 `return plan->waypoints;` | 有；`#1 validate_plan (plan=0x0) at ...:36`、`#2` 回调 lambda `at ...:82`；`info locals` 显示 `No locals.`（参数在帧里已经显示了） |
| `Release` | `std::_Function_handler<void (std::shared_ptr<std_srvs::srv::Trigger_Request_...` | 无；`count_waypoints`、`validate_plan` 被内联，整个回溯只有 9 帧 |
| `RelWithDebInfo` | 回调 lambda `operator()`，源码行 `82  res->success = validate_plan(plan) > 0;` | 有行号；`info locals` 显示 `plan = 0x0`、`this = 0x...` |

四种都是 `received signal SIGSEGV`。

退出码是 **0**：gdb 把崩溃"接住"了，打印完回溯正常退出。CI 里用这种方式抓回溯时不能只看退出码。

所有回溯的下半部分都一样：`rclcpp::AnyServiceCallback<std_srvs::srv::Trigger>::dispatch` → `rclcpp::Service<...>::handle_request` → `rclcpp::Executor::execute_service` → `execute_any_executable` → `SingleThreadedExecutor::spin` → `rclcpp::spin` → `main`。这一段告诉你"这是一个服务回调，由单线程执行器调度"。

</details>

## 3. 交互式 gdb

```bash
source /tmp/bt/Debug/install/setup.bash
ros2 run --prefix 'gdb -ex run --args' observe_lab fake_planner --ros-args -p crash_on_replan:=true
# 触发崩溃后，在 (gdb) 提示符下
bt
frame 2
print replans_
print *this
info threads
thread apply all bt 3
```

想一想：`frame 2` 里为什么能 `print replans_`？`info threads` 里有几个线程，它们是谁创建的？

<details>
<summary>参考结果（填完预测再展开）</summary>

🔍 `frame 2` 是服务回调 lambda（源码第 82 行），它捕获了 `this`，所以能直接看成员：`print replans_` 得到 `$1 = 1`（回调里先 `++replans_` 再崩溃）。

🔍 `info threads` 实测 18 个线程：

- `Thread 1 "fake_planner"`：主线程，运行执行器，标 `*` 的就是收到 SIGSEGV 的那个；
- 2 个 `fake_planne-ust`：LTTng-UST 的监听线程（`tracetools` 链接了 lttng-ust，没开追踪也会创建）；
- 11 个 `dds.*`（`dds.ev.0`、`dds.udp.7400`、`dds.shm.7411`、`dds.asyn.0.0` 等）：Fast DDS 的事件、UDP/共享内存接收、异步发送线程，端口号对应 Domain 0 的发现和用户数据端口；
- 其余 4 个 `fake_planner`：💡 推测是 rclcpp 的信号处理线程以及 RMW、rcl 内部的辅助线程，没有逐个确认。

💡 多线程执行器时，执行器线程也叫进程名，要靠 `thread apply all bt 3` 看各自停在哪里来区分。

</details>

## 4. 用 LTTng 追踪回调

```bash
lttng create plan_trace --output=/tmp/traces/plan_trace
lttng enable-event -u 'ros2:*'
lttng add-context -u -t vtid -t procname
lttng start
ros2 run observe_lab fake_planner --ros-args -p work_ms:=30.0 -p rate_hz:=10.0   # 5 秒后 Ctrl-C
lttng stop && lttng destroy

babeltrace2 /tmp/traces/plan_trace | wc -l
babeltrace2 /tmp/traces/plan_trace | grep -o 'ros2:[a-z_]*' | sort | uniq -c | sort -rn
ros2 run observe_lab callback_durations.py /tmp/traces/plan_trace
```

| # | 问题 | 预测 | 实测 |
|---|---|---|---|
| 1 | `callback_start` 大约多少条？为什么不是 50？ | | |
| 2 | 回调平均时长、触发周期 | | |
| 3 | 先启动节点、再 `lttng start`，结果有什么不同？ | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

1. 一次约 5 秒的实测：752 行事件，`callback_start` 和 `callback_end` 各 47 条。10 Hz × 约 5 秒，减去启动和退出的那一点时间。最多的是 `rmw_publish`（139）、`rcl_publish`（109）、`rclcpp_publish`（108）：每次定时器回调发两条消息（`/plan`、`/plan_markers`），每一层各记一条。
2. 另一次运行（约 5 秒）：`52 calls  mean  30.197 ms  max  30.409 ms  period  100.0 ms  FakePlanner::FakePlanner()::{lambda()#1}`。
3. 💡 `rclcpp_callback_register` 等初始化事件已经错过，脚本里只剩回调地址（如 `0x561ddf88c078`），没有符号名。时长统计不受影响。

</details>

## 5. 回调比周期长会怎样

```bash
for w in 0.0 30.0 120.0 250.0; do
  # 每次重复第 4 部分的 lttng create/enable/add-context/start
  ros2 run observe_lab fake_planner --ros-args -p work_ms:=$w -p rate_hz:=10.0 &
  sleep 1.5; timeout 3 ros2 topic hz /plan; kill -INT %1; wait
  # lttng stop/destroy，再运行 callback_durations.py
done
```

| `work_ms` | 预测：回调时长 | 预测：实际周期 / `hz` | 实测 |
|---|---|---|---|
| 0 | | | |
| 30 | | | |
| 120 | | | |
| 250 | | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

| `work_ms` | 回调平均 / 最大 | 实际周期 | `ros2 topic hz /plan` |
|---|---|---|---|
| 0 | 0.180 / 0.499 ms | 100.0 ms | 9.999 Hz |
| 30 | 30.197 / 30.409 ms | 100.0 ms | 9.999 Hz |
| 120 | 120.191 / 120.295 ms | 120.3 ms | 间隔 0.120–0.121 s（约 8.3 Hz） |
| 250 | （未追踪） | — | 3.994 Hz，间隔 0.208–0.292 s |

回调比周期长时，定时器在回调结束后立即再次触发，不会补发错过的次数，实际频率 ≈ 1 / 回调时长。频率悄悄下降，节点不会打印任何警告。

</details>

## 挑战（选做）

1. 用 `backward_ros` 代替 gdb：在 `observe_lab` 的 `package.xml` 和 `CMakeLists.txt` 里加上依赖（📄 只需 `find_package(backward_ros REQUIRED)`），触发崩溃，对比它自动打印的回溯和 gdb 的回溯。
2. 把 `fake_planner` 改成多线程执行器，并给 `/replan` 服务一个单独的回调组，让 `/replan` 回调里也 `busy_wait` 200 ms。用 tracing 证明：定时器回调不再被 `/replan` 阻塞（两个回调在不同 `vtid` 上重叠）。
3. 扩展 `callback_durations.py`：对每个回调输出时长的 99 分位数，并统计"间隔 > 1.5 × 周期"的次数。用它对比第 4 批[实时循环的抖动](realtime-jitter.md)里的 `--load` 场景。
