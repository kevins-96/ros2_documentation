# 答案：rmw 接口与序列化的设计

> 对应：[rmw-serialization-design.md](../design/rmw-serialization-design.md)

## 第 1 题

**（a）** 如果编译时就选定中间件，构建农场要为 M 种中间件 × N 个包各编一套二进制，维护成本和算力成本都太高 📐。所以选择在运行时选：

- 编译每个消息包时，把当时能找到的所有"类型支持生成器"的代码都生成进去 📐。
- `rmw_implementation` 在运行时按环境变量加载选定实现的共享库，再把调用转过去 📐。

代价就在每个消息包里：例如 `std_msgs` 装了三组库 🔍，

- `libstd_msgs__rosidl_typesupport_c/cpp.so`：分发层，负责在运行时找到具体的类型支持；
- `libstd_msgs__rosidl_typesupport_fastrtps_c/cpp.so`：Fast DDS 专用；
- `libstd_msgs__rosidl_typesupport_introspection_c/cpp.so`：通用的内省类型支持。

这多出来的库就是"支持运行时切换"的成本。

**（b）** 用同一套 `.deb` 包。消息包在构建农场上已经带了多种类型支持；两边的差别只在于各自装了哪个 `rmw_*` 包，以及 `RMW_IMPLEMENTATION` 设成什么（不设时默认 Fast DDS）📄 📐。💡 Cyclone DDS 的 RMW 用的是内省类型支持，所以消息包里不需要 Cyclone 专用的代码。

注意：能用同一套包，不等于两台机器之间能互通。不同厂商的 DDS 之间互通是"不保证"的 📄（见卡片坑 1），一个系统最好统一用一种 RMW。

## 第 2 题

**（a）** 最可能的瓶颈是**序列化和反序列化**，不是那次内存拷贝 🔍 💡。

- 每个 `PoseStamped` 里有 `header.stamp`、`header.frame_id`（字符串）和 7 个 `double`。20000 个就是大量又小又碎的字段，还夹着字符串。
- 实测 1 MB 的 `PoseArray`（每个位姿 7 个 `double`，没有字符串），序列化就比拷贝慢 15.9 倍，单次约 0.8 ms，反序列化也差不多 🔍。`Path` 每个元素多了一个 header，只会更慢 💡。
- 设计文档"慢两个数量级"的说法对平坦数组不成立（只慢 1.2 倍），对这种碎字段的结构更接近，但实测也只有一个数量级左右 🔄。

按每秒 10 次计算，发布端序列化加接收端反序列化每秒要花十几到几十毫秒的 CPU，再加上传输 💡。

**（b）** 两种都不改控制器算法的办法：

1. **让控制器和规划器在同一个进程里，走进程内通信**：做成组件放进同一个容器，发布 `unique_ptr`，控制器用 `ConstSharedPtr` 回调。这样消息根本不序列化，直接传指针 📐 🔍。依据：[进程内通信与零拷贝的设计](../design/intra-process-design.md)，新实现完全绕开中间件。
2. **改消息结构，把路径打包成平坦数组**：例如定义 `float64[] xs`、`float64[] ys`、`float64[] yaws`，只带一个 header，或者先降采样再发布。连续的基本类型数组序列化几乎就是一次 `memcpy`（实测只比拷贝慢 1.2 倍）🔍。依据：本篇"序列化比拷贝慢多少要看消息结构"这一行。

借用消息帮不上忙：`Path` 里有字符串和变长数组，不是平坦类型 🔍。
