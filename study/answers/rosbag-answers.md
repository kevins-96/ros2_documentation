# 答案：rosbag 录制回放

> 先在[卡片](../cards/observability/rosbag.md)里自己写下答案再来对照。

## 题 1：回放时 tf2 报 `extrapolation into the past`

**原因 1：时间基准不一致。** bag 里的消息 `header.stamp` 是录制当天的时间，`/tf` 里的变换也是。规划器如果用墙钟（`use_sim_time` 为 false），它用"现在"去查 tf，buffer 里只有"过去那天"的数据，查询时间落在 buffer 范围外，就报外推错误。

改法：回放加 `--clock`，规划器（以及同时运行的其他节点，比如 RViz、robot_state_publisher）都设 `use_sim_time:=true`。🔍 `--clock` 发布的 `/clock` 是 best effort、volatile。

**原因 2：`/tf_static` 没收到。** 静态变换只在启动时发一次（transient local）。📄 rosbag 能录到录制开始前发的 transient local 消息，🔍 回放时也用 transient local 发布，所以通常没问题。但如果 bag 被分割（`-d`、`-b`），后面的文件里没有 `/tf_static`，从中间开始放（`--start-offset` 或只放第二个文件）就会缺静态变换，表现是缺 `base_link -> laser` 这类变换。改法：录制时加 `--repeat-transient-local /tf_static`（🔍 用 `/map` 实测过：加了之后每个分割文件开头都补写了一条），或者从头放，或者回放时另起一个 `static_transform_publisher` 补上。

💡 还有一种常见错误：同时运行了真实的 tf 发布者（比如真车驱动的仿真版本）和 bag 里的 `/tf`，两个来源的时间对不上。回放时只放 bag 里的 tf，别再启动会发 tf 的节点。

## 题 2：出错时把最近 10 秒的 `/plan` 和 `/odom` 存成 bag

**消息怎么存：** 两个订阅都用"模板写真实类型、回调收 `SerializedMessage`"的写法（`create_subscription<nav_msgs::msg::Path>` + `std::shared_ptr<const rclcpp::SerializedMessage>` 回调），把 `(话题名, 类型字符串, 收到时间, 序列化消息)` 放进一个按时间淘汰的环形缓冲（`std::deque`，每次插入后弹出早于 `now - 10s` 的条目）。存序列化消息不用反序列化，开销小。

**出错时怎么写盘：** 创建新的 `rosbag2_cpp::Writer`，`open()` 一个带时间戳的新目录（目录已存在会抛异常），按顺序对缓冲里每条调用 `writer.write(msg, topic, type, time)`，然后让 Writer 析构（离开作用域或 `reset()`），这时才写 `metadata.yaml`。💡 写盘可能花几十毫秒，别在控制回路的回调里做：放到单独线程或单独的回调组。

**两个坑：**

1. 🔍 订阅模板参数写成 `rclcpp::SerializedMessage` 会在运行时抛 `this specialization of rclcpp::get_message_type_support_handle() should never be called`。
2. 🔍 进程在 Writer 析构前崩溃，bag 里只有数据文件、没有 `metadata.yaml`，`ros2 bag info` 打不开。"出错时存 bag"恰恰常发生在程序快要崩溃的时候：写完立即让 Writer 析构，不要把 Writer 留到进程退出时才释放。

另外：话题名写完整的 `/plan`、`/odom`，否则 bag 里是相对名，按 `/plan` 过滤找不到（🔍 练习里的 `plan_recorder` 就是这样）。

💡 不想自己写的话，🔍 Rolling 的 `ros2 bag record --snapshot-mode` 加服务 `/rosbag2_recorder/snapshot` 就是这个功能（缓冲大小用 `--max-cache-size` 控制；本次未实测）。自己写的好处是能在规划器内部的"出错点"触发，并且只存相关话题。
