# 答案：进程内通信与零拷贝的设计

> 对应：[intra-process-design.md](../design/intra-process-design.md)

## 第 1 题

**（a）** 两个订阅者都要所有权（`UniquePtr` 回调，默认缓冲区跟回调一致），$N=2$，复制 $N-1=1$ 次 📐。一个拿到原件，另一个拿到副本 🔍。拿到原件的是哪一个，不要依赖：文档说这是不确定的 📄（见卡片坑 2）。

**（b）** 关键在于代价地图只读点云，不改它。

- **把代价地图的回调改成 `ConstSharedPtr`** 💡（推导自设计文档的复制表）。这时 $M=1$ 个要所有权、1 个共享：障碍物检测拿原件，代价地图共享一份副本，仍是 1 次复制 📐。如果障碍物检测也只读，两个都改成 `ConstSharedPtr`，就是 **0 次复制** 📐 🔍（卡片里 2 个 `ConstSharedPtr` 订阅者地址相同）。
- **只把代价地图的缓冲区设成 `SharedPtr` 反而更糟** 💡。按设计文档的表，`unique + shared` 两种缓冲区在推送时就要复制 1 次；代价地图的 `UniquePtr` 回调从 shared 缓冲区取消息时还要再复制一次。"缓冲区设成 `SharedPtr` 推迟复制"这个技巧，只在**所有**订阅者都要所有权、而且大多数消息会被覆盖时才划算。实测两个 `UniquePtr` 订阅者都用 `SharedPtr` 缓冲区时，两个都拿到副本，比默认还多一次 🔍。

**（c）** 会变。有了进程外的订阅者，发布时要同时走进程内和进程间 📐：

- `unique_ptr` 先转成 `shared_ptr`，交给 RMW 序列化发出去。
- 每个要所有权的进程内订阅者**都**要复制一份，不能再把原件交出去。
- 所以两个 `UniquePtr` 订阅者时，从 1 次复制变成 2 次，另外再加一次序列化 📐 💡。

如果按（b）改成 `ConstSharedPtr`，进程内就还是 0 次复制，只多一次序列化。

## 第 2 题

**（a）** 发布端能借用时，`publish(LoanedMessage)` 只调用 `rcl_publish_loaned_message`，**完全不经过 IntraProcessManager** 🔍（rclcpp 源码）。消息走 RMW，同进程的订阅者照样能收到，但实测地址和借出的地址不同 🔍。所以两个开关一起开并不会"更快"，进程内通信对这条消息根本不起作用 💡。

**（b）** rcl 的默认值是：订阅者 `disable_loaned_message = true`，只有 `ROS_DISABLE_LOANED_MESSAGES=0` 时才打开 🔍（`rcl/subscription.h`）。发布者正好相反，默认能借，`ROS_DISABLE_LOANED_MESSAGES=1` 才关。所以设 `ROS_DISABLE_LOANED_MESSAGES=0` 再启动，订阅端就能借了 🔍。这和设计文档"默认尝试借用"的说法不同 🔄。

**（c）** 不能 💡。`PointCloud2` 里有 `header.frame_id`（`string`）、`fields`（变长数组，元素里还有 `string name`）和 `data`（变长字节数组），不是"平坦"的定长类型。实测连 `String` 都借不了 🔍，文档也要求 Fast DDS 下借用的消息是 POD 类型 📄（见卡片坑 8）。要借用，就得像本仓库的 `FixedBlock` 那样，自己定义一个只有基本类型和定长数组的消息 🔍。
