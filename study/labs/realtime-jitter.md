# 练习：实时循环的抖动

> 适用：ROS 2 **Rolling**（rclcpp 33.2.0）。对应卡片：[实时编程](../cards/concurrency/realtime.md)
> 代码：[rt_lab.cpp](perf_lab/src/rt_lab.cpp)
> 用时：约 30 分钟

## 0. 编译

同[组件练习](component-container.md)。

`rt_lab` 跑一个周期循环（默认 1 kHz、3000 个周期），统计每次唤醒比计划时刻晚了多少，以及运行期间的 minor 缺页次数。

| 参数 | 作用 |
|---|---|
| `--loop sleep` | 用 `clock_nanosleep` 绝对时间睡眠（默认） |
| `--loop timer` | 用 rclcpp `create_wall_timer` + 单线程执行器，统计相邻两次回调的间隔 - 周期 |
| `--alloc N` | 每个周期分配并写满 N 字节的 `std::vector<char>` |
| `--lock` | 启动时 `mlockall(MCL_CURRENT \| MCL_FUTURE)` 并预先摸一遍栈 |
| `--fifo P` | 把线程设成 `SCHED_FIFO`、优先级 P |
| `--load N` | 另开 N 个忙等线程抢 CPU |
| `--period-us`、`--iters` | 周期和次数 |
| `--count-new` | 不跑循环，改为统计每次发布调用多少次 `operator new` |

先查一下你机器的核数（`nproc`）和权限（`ulimit -l`、`ulimit -r`）。

## 1. 先填预测

| # | 命令（都加在 `ros2 run perf_lab rt_lab` 后面） | p99 比基线高还是低 | 缺页多还是少 | 实测 |
|---|---|---|---|---|
| 1 | （无参数，基线） | — | — | |
| 2 | `--alloc 65536` | | | |
| 3 | `--alloc 4000000` | | | |
| 4 | `--alloc 4000000 --lock` | | | |
| 5 | `--fifo 80` | | | |
| 6 | `--load <核数>` | | | |
| 7 | `--load <2×核数>` | | | |
| 8 | `--loop timer` | | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

2026-11-17 在 4 核云主机的 Docker 容器里实测（普通内核，`ulimit -l` 为 8192 kB，`ulimit -r` 为 0）：

| # | p50 | p99 | 最大 | 缺页 | 备注 |
|---|---|---|---|---|---|
| 1 | 122 μs | 303 μs | 4.4 ms | 8 | |
| 2 | 120 μs | 396 μs | 3.5 ms | 25 | |
| 3 | 106 μs | **1175 μs** | 3.1 ms | **1962** | p99 超过了 1 ms 周期 |
| 4 | 109 μs | 524 μs | 2.6 ms | 1963 | `mlockall failed: Cannot allocate memory`，没锁住 |
| 5 | 109 μs | 307 μs | 8.3 ms | 8 | `SCHED_FIFO 80: Operation not permitted`；加 `--cap-add SYS_NICE` 也一样 |
| 6 | **70 μs** | 287 μs | 3.6 ms | 7 | 4 个忙等线程，反而更快 |
| 7 | 69 μs | **4879 μs** | 8.9 ms | 9 | 8 个忙等线程抢 4 个核 |
| 8 | -1.2 μs | 158 μs | 4.0 ms | 28 | 间隔偏差；3 秒回调 3000 次，按时长应为 3006 次 |

解读：
- 3：每周期分配 4 MB 导致大量缺页，最坏延迟明显变差。2 的缺页少，是因为 glibc 复用了堆上的空闲块，但这是实现细节，不能依赖。
- 4、5：没有权限时这两个调用都会失败，程序却照常运行。一定要检查返回值。
- 6：💡 CPU 有事做时不进入深度睡眠（C-state）、不降频，唤醒更快。实时系统常关掉深度 C-state。
- 7：线程数超过核数时，循环线程要和忙等线程轮流用 CPU，p99 涨了十几倍。💡 这正是 `SCHED_FIFO` 要解决的：实时线程优先级更高，能抢占普通线程。本环境没有权限验证，请在你自己的机器上用 `sudo` 或配置好 `limits.conf` 后试 `--load 8 --fifo 80`。
- 8：rclcpp 定时器晚了超过一个周期时会跳过错过的周期，不会补发。

</details>

## 2. 发布一条消息分配几次内存

```bash
ros2 run perf_lab rt_lab --count-new
```

<details>
<summary>参考结果（填完预测再展开）</summary>

```text
Int32: make_unique + publish(move)              1.07 new/publish
Int32: preallocated, publish(const &)           0.00 new/publish
String(64 chars): make_unique + publish(move)   2.00 new/publish
String(64 chars): preallocated, publish(const &)   0.08 new/publish
```

- `make_unique` 本身就是一次分配；`String` 再加上字符串内容的一次。
- 复用一个预先分配好的消息、按引用发布，rclcpp 层几乎不分配。
- 这里只统计了 C++ `operator new`。📄 DDS 内部的分配（多数用 `malloc`）不在里面；文档的分配器教程里，自定义分配器只接住了约 2/3。

</details>

## 挑战（选做）

1. 如果你有 root 权限：在 `/etc/security/limits.conf` 里加 `memlock -1` 和 `rtprio 98`，重新登录后重做 4、5、7，以及 `--load 8 --fifo 80`。
2. 把 `--period-us` 改成 10000（100 Hz），基线的 p99 和最大值相对周期还危险吗？这对"导航控制器用 rclcpp 定时器够不够"意味着什么？
3. 用 `taskset -c 3 ros2 run perf_lab rt_lab --load 3` 把循环固定在第 3 核，忙等线程会被调度到哪里？结果有什么变化？
