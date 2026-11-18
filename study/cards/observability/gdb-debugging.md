---
tags: [ros2, gdb, backtrace, 调试, debug, IDE]
distro: rolling
status: batch-6
aliases: [调试与回溯, gdb, backtrace, 回溯, 段错误, segfault, ros2 run --prefix, launch prefix, CMAKE_BUILD_TYPE, RelWithDebInfo, VS Code 调试, backward_ros]
---

# 调试与回溯：gdb、构建类型与 IDE

> 适用：ROS 2 **Rolling**（2026-11-17 在 `ros:rolling-ros-base` 容器里实测，gdb 17.1，GCC 15）
> 来源标记：📄 仓库文档原文 · 🔍 源码或实测核实 · 💡 经验补充（可信，但请自己验证）
> 源文档：[About-Debugging.rst](../../../source/Developer-Tools/About-Debugging.rst)、[Getting-Backtraces-in-ROS-2.rst](../../../source/Developer-Tools/Debugging/Getting-Backtraces-in-ROS-2.rst)、[ROS-2-IDEs.rst](../../../source/Developer-Tools/Debugging/ROS-2-IDEs.rst)

## 1. 一句话定义

回溯（backtrace）是程序崩溃那一刻的**调用栈快照**：从崩溃的函数（第 0 帧）一层层列到 `main`。在 ROS 2 里用 `ros2 run --prefix 'gdb -ex run --args'` 或 launch 的 `prefix` 把节点放进 gdb 运行，崩溃时 `bt` 就能拿到它。📄

## 2. 为什么需要

节点段错误时，`ros2 run` 只打印一行：

```text
[ros2run]: Segmentation fault          # 🔍 退出码 245（即 -11，SIGSEGV）
```

💡 规划器里最常见的崩溃：空指针（规划失败返回 `nullptr` 没检查）、越界（栅格坐标没做边界检查）、迭代器失效（回调里改了容器）、悬空引用（lambda 捕获了已析构的对象）。一行 `Segmentation fault` 什么都说明不了，回溯能直接指到文件和行号。

### 怎么读回溯

🔍 `fake_planner` 在 `/replan` 回调里解引用空指针，Debug 构建下 `bt` 的前几帧：

```text
Thread 1 "fake_planner" received signal SIGSEGV, Segmentation fault.
#0  (anonymous namespace)::count_waypoints (plan=0x0) at .../fake_planner.cpp:31   ← 崩溃点
#1  (anonymous namespace)::validate_plan (plan=0x0) at .../fake_planner.cpp:36     ← 谁调用了 #0
#2  FakePlanner::FakePlanner()::{lambda(...)#1}::operator()(...) at .../fake_planner.cpp:82  ← 你的服务回调
#3-#6  std::__invoke_impl / std::function ...                                       ← 标准库包装，跳过
#7  rclcpp::AnyServiceCallback<std_srvs::srv::Trigger>::dispatch(...)
#8  rclcpp::Service<std_srvs::srv::Trigger>::handle_request(...)
#9  rclcpp::Executor::execute_service(...)   from /opt/ros/rolling/lib/librclcpp.so
#11 rclcpp::executors::SingleThreadedExecutor::spin()
#14 main (argc=4, argv=...) at .../fake_planner.cpp:195
```

- **第 0 帧是崩溃发生的地方**，往下是一层层的调用者，最底下是 `main`。📄 文档说"从下往上读"，意思是从 `main` 读起能看懂"程序是怎么走到这里的"；💡 实际排查时先看第 0 帧和**第一个属于你代码的帧**（这里 #0–#2），再往下看是哪个回调、哪个执行器触发的。
- 参数值直接显示在帧里：`plan=0x0` 一眼看出空指针。
- `#7`–`#13` 这一段（`AnyServiceCallback::dispatch` → `Service::handle_request` → `Executor::execute_service` → `spin`）在每个 ROS 2 回调崩溃里都长这样，可以当"路标"：它告诉你这是哪类回调（服务、订阅、定时器）。

### 构建类型决定你能看到什么

🔍 同一个崩溃，四种 `CMAKE_BUILD_TYPE`：

| 构建类型 | 优化 / 调试信息 | 第 0 帧 | 行号与变量 |
|---|---|---|---|
| 不设（colcon 默认） | 无优化，无 `-g` | `count_waypoints(...)`，函数名完整 | **没有行号**；`info locals` 报 `No symbol table info available.` |
| `Debug` | `-O0 -g` | `count_waypoints (plan=0x0) at fake_planner.cpp:31` | 行号、参数值都有 |
| `Release` | `-O3`，无 `-g` | 直接是 `std::_Function_handler<...>::_M_invoke`：`count_waypoints`、`validate_plan` **被内联消失** | 没有行号，只剩 9 帧 |
| `RelWithDebInfo` | `-O2 -g` | 回调 lambda，`at fake_planner.cpp:82`（调用点那一行） | 有行号，`info locals` 能看到 `plan = 0x0`；被内联的函数不单独成帧 |

📄 不加 `-g` 也能拿到回溯，但没有行号。💡 现场部署建议用 `RelWithDebInfo`：性能接近 Release，崩溃时还能定位到行。

## 3. 最小用法（命令和 launch）

```bash
# 1. 带调试信息编译（只编你关心的包）
colcon build --packages-select observe_lab --cmake-args -DCMAKE_BUILD_TYPE=Debug

# 2. 在 gdb 里运行节点：-ex run 让 gdb 启动后立即运行，--args 后面原样是程序和参数
ros2 run --prefix 'gdb -ex run --args' observe_lab fake_planner --ros-args -p crash_on_replan:=true

# 3. 另一个终端触发崩溃
ros2 service call /replan std_srvs/srv/Trigger

# 4. 回到 gdb 提示符
(gdb) bt                 # 回溯
(gdb) frame 2            # 切到第 2 帧（你的回调）
(gdb) info locals        # 看局部变量
(gdb) thread apply all bt   # 多线程执行器：所有线程的栈
```

非交互（CI、脚本）时用 `gdb -q -batch -ex run -ex bt --args`，崩溃后自动打印回溯并退出。

launch 文件里（📄）：

```python
Node(package='observe_lab', executable='fake_planner',
     prefix=['xterm -e gdb -ex run --args'],   # 有桌面：单独窗口里交互
     # prefix=['gdb -ex run --args'],          # 无桌面：和 launch 日志混在一个终端
     parameters=[{'crash_on_replan': True}])
```

📄 大型 launch：把要调试的节点从父 launch 里注释掉，父 launch 照常启动，再单独用 `ros2 run --prefix ...` 起这个节点，用 `--ros-args -r __node:=<名字> --params-file <绝对路径>` 补回原来的参数和重映射（见[启动参数与重映射](../configuration/node-arguments.md)）。

📄 gtest 失败：直接 `gdb -ex run ./build/<包>/<测试可执行文件>`；未捕获异常用 `catch throw` 在抛出点停下。📄 不想手动开 gdb：`backward_ros` 作为依赖加进来，崩溃时自动打印回溯。

### IDE（📄 要点）

- VS Code 要从**已 source 的终端**里启动（`code .`），否则 IntelliSense 和调试器找不到 ROS 的头文件和库。
- C++ 调试同样需要 `-DCMAKE_BUILD_TYPE=Debug`；💡 加 `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` 让编辑器用和 colcon 一致的编译参数。
- Python 节点：colcon 默认把源码**复制**到 `install/`，你在源码里打的断点不会命中；用 `--symlink-install` 或者在 install 下的文件里打断点。📄 VS Code 调试 Python 节点时直接用 `python 文件.py` 形式的配置比 `ros2 run` 简单；也可以"按进程 ID 附加"。

## 4. 常见坑

1. **gdb 批处理模式会吞掉崩溃退出码。** 🔍 `ros2 run --prefix 'gdb -q -batch -ex run -ex bt --args' ...` 崩溃后 `ros2 run` 的退出码是 **0**（gdb 正常结束），不带 gdb 时是 245。💡 CI 里用 gdb 包一层抓回溯时，要另外判断"有没有 `received signal`"，不能只看退出码。
2. **没有设 `CMAKE_BUILD_TYPE` 不等于 Debug。** 🔍 colcon 默认（不设）没有 `-g`：函数名有，行号和变量没有。📄 想要行号必须显式 `-DCMAKE_BUILD_TYPE=Debug`（或 `RelWithDebInfo`）。
3. **构建类型会被 CMake 缓存。** 📄 改构建类型后要 `--cmake-clean-cache`（或删 `build/<包>`）。🔍 第 5 批也遇到过同样的缓存问题：`-D` 选项不带时沿用上次的值（见[测试与代码规范检查](../../labs/testing-and-lint.md)第 2 部分）。💡 反过来也成立：用 Debug 调完忘了改回来，现场跑的是 `-O0`，CPU 占用会高很多。
4. **Release 下"崩溃的函数"可能根本不出现。** 🔍 `count_waypoints` 被内联后，第 0 帧变成 `std::_Function_handler<...>::_M_invoke`。看到第 0 帧是 `std::function` 包装或者回调 lambda，要想到真正出错的可能是被内联进去的小函数。
5. **空指针解引用是未定义行为，不保证在那一行崩溃。** 💡 本例四种构建都在同一处 SIGSEGV，但优化器有权假设指针非空并重排代码；有时崩溃会"漂移"到别处，甚至不崩溃而得到错误结果。修复要从源头（检查规划结果）入手，而不是只看崩溃点。
6. **容器里 gdb 会提示 `Error disabling address space randomization: Operation not permitted`。** 🔍 Docker 默认安全策略不允许关 ASLR，不影响拿回溯；需要可复现的地址时 `docker run --cap-add=SYS_PTRACE --security-opt seccomp=unconfined`（💡 未实测）。注意 `seccomp=unconfined` 会关掉容器的整套系统调用过滤，明显削弱隔离，只在本机调试用的临时容器里加，不要写进部署用的镜像或 compose 文件。
7. **多线程执行器崩溃时，`bt` 只显示当前线程。** 💡 用 `thread apply all bt`；崩溃线程会标出 `received signal` 的那个。

## 5. 与其他概念的关系

- [工作空间与 colcon](../engineering/workspace-colcon.md)：`--cmake-args`、`--symlink-install`、`--packages-select`
- [ament_cmake](../engineering/ament-cmake.md)：`CMAKE_BUILD_TYPE` 和编译选项
- [launch 启动系统](../configuration/launch.md)：`prefix` 参数
- [执行器](../concurrency/executor.md)：回溯里的 `Executor::execute_*` 帧说明了是哪种回调
- [tracing 性能追踪](tracing.md)：gdb 看"崩在哪"，tracing 看"慢在哪"
- 练习：[崩溃回溯与回调追踪](../../labs/crash-and-trace.md)
- 上一级：[总览脑图](../../map/overview.md)

## 6. 自测题（先自己答，再看[答案](../../answers/gdb-debugging-answers.md)）

1. 现场机器人上的局部规划器偶尔段错误，现场用的是 Release 构建，你拿到的回溯第 0 帧是 `std::_Function_handler<void (std::shared_ptr<nav_msgs::msg::Odometry const>), ...>::_M_invoke`，没有行号。你能从中读出什么？下一步怎么做才能拿到更有用的回溯，同时不明显影响现场性能？
2. 同事说："我加了 `--cmake-args -DCMAKE_BUILD_TYPE=Debug` 重新编译，gdb 里还是没有行号。" 列出至少两个可能原因，以及各自怎么检查。
