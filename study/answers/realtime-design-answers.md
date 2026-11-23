# 答案：实时系统的设计

> 对应：[realtime-design.md](../design/realtime-design.md)

## 第 1 题

**（a）固实时（firm real-time）** 📐。错过截止期不算系统失效，但这一周期迟到的结果**作废不用**，退回上一周期的指令，这正是固实时的定义。如果迟到的结果照样拿来用、只是控制效果变差，那才是软实时。硬实时则是"错过一次就算系统失效"，例如安全控制器错过一次就要急停。

**（b）** 预测时域是参数、启动后不变，属于"运行期才知道，但在进入实时段之前就知道"这一行 📐。办法是在初始化段一次性分配：

- 在 `on_configure`（或构造函数）里读出预测时域，按最大长度 `reserve` 或 `resize` 好所有向量和矩阵 💡。
- 循环里只在容量以内 `resize`。`std::vector::resize` 到不超过 `capacity()` 的长度时不会重新分配 💡（标准库保证）。最稳妥的做法是循环里根本不改大小。
- 可以用 `rt_alloc_probe` 的办法替换全局 `operator new`，在单元测试里断言循环里的分配次数为 0 🔍。

**（c）** 自定义分配器只接管了**消息本身**那一次分配 🔍。实测在 Rolling + Fast DDS 上：

- 进程间路径：每条消息还有约 3 次系统 `new`，调用栈落在 Fast DDS 的 `DataReaderHistory::add_to_instance` 和 rmw 的 take 路径（`rcl_take` 之下）🔍。
- 进程内路径：每条消息还有约 3 次系统 `new`，位置在 `IntraProcessManager::do_intra_process_publish` 和 `SubscriptionIntraProcess` 里 🔍。
- 服务、客户端、参数根本没有分配器参数 🔍。

设计文档列的"rmw 全面实现分配器接口"还没完成 📐 ⏳。

## 第 2 题

**（a）** 24 字节的 `Pose3d` 在 x86-64 上 `is_lock_free()` 为 0 🔍。这时 GCC 调用 libatomic，用全局锁表来实现 `load`、`store` 💡，等于藏着一把锁。如果低优先级的定位线程正好持有这把锁时被中等优先级的线程抢占，`SCHED_FIFO` 的控制线程就会一直等下去，这就是设计文档讲的**优先级反转（priority inversion）** 📐。代码里看不到锁，所以更难排查。

**（b）** 设计文档给的办法有：不用阻塞的同步原语、持锁时禁止抢占、提高持锁任务的优先级、优先级继承、无锁数据结构 📐。下面是两种可行方案 💡：

- **seqlock（无锁读）**：
  - 写线程：先把序号加 1（变成奇数），写入 `x, y, yaw`，再把序号加 1（变回偶数）。
  - 读线程：读序号，若是奇数就重来；拷出三个字段；再读一次序号，和第一次不同就重来。
  - 读线程永远不会阻塞写线程，写线程也不会阻塞读线程。最坏情况下读线程要重试，但定位 50 Hz、控制 1 kHz 时几乎不会重试。

```cpp
struct SeqPose {
  std::atomic<uint32_t> seq{0};
  std::atomic<double> x{0}, y{0}, yaw{0};   // 每个字段本身无锁
  void write(double nx, double ny, double nyaw) {
    seq.fetch_add(1, std::memory_order_acq_rel);          // 变奇数：正在写
    x.store(nx, std::memory_order_relaxed);
    y.store(ny, std::memory_order_relaxed);
    yaw.store(nyaw, std::memory_order_relaxed);
    seq.fetch_add(1, std::memory_order_release);          // 变偶数：写完
  }
  Pose3d read() const {
    for (;;) {
      uint32_t s0 = seq.load(std::memory_order_acquire);
      if (s0 & 1u) {continue;}
      Pose3d p{x.load(std::memory_order_relaxed), y.load(std::memory_order_relaxed),
        yaw.load(std::memory_order_relaxed)};
      std::atomic_thread_fence(std::memory_order_acquire);
      if (seq.load(std::memory_order_relaxed) == s0) {return p;}
    }
  }
};
```

- **优先级继承互斥锁**：用 `pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT)` 创建互斥锁，临界区只拷三个 `double`。低优先级线程持锁时会临时继承控制线程的优先级，不会被中等优先级的线程抢占。这个办法简单，但控制线程仍然可能等一小段时间。
