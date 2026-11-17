# 答案：tf2 与时间

> 先在[卡片](../cards/coordinates/tf2-time.md)里自己写下答案再来对照。

## 题 1：点云投影报 "extrapolation into the future"

**原因：** `now()` 时刻的变换还没进 buffer（广播有几毫秒延迟），tf2 只插值不外推。📄 🔍 练习里 `--mode now` 79 次全部失败。

**改成 `TimePointZero` 行不行？** 能消除报错，但语义变了：用的是"最新的变换"，而不是"这帧点云采集时的变换"。💡 推导：

- 机器人静止或慢速直行时，误差可以忽略；
- 机器人快速转弯时出错。例如角速度 1 rad/s，点云从采集到处理延迟 100 ms，最新变换和采集时刻差约 5.7°，5 m 外的点横向偏约 0.5 m，代价地图里障碍物会"拖影"或错位。
- 如果 `laser → base_link` 是静态的、只要投到 `base_link`，用 `TimePointZero` 没问题（静态变换对所有时刻都有效）。问题出在经过 `odom → base_link` 这段动态变换时。

**更好的方案：**

1. 用点云自己的时间戳加超时：`lookupTransform("odom", "laser", msg->header.stamp, 50ms)`；注意不要在和广播者共用的单线程执行器里阻塞（卡片坑 2）。
2. 更推荐 `tf2_ros::MessageFilter`：点云先缓存，等对应时刻的变换到达再回调，不阻塞执行器，超时自动丢弃。📄

## 题 2：跟随领航者 3 秒前的位置

```cpp
rclcpp::Time now = this->now();
auto t = buffer_->lookupTransform(
  "follower/base_link", now,              // 在"现在的我"里看
  "leader/base_link", now - 3s,           // "3 秒前的领航者"
  "map",                                  // 固定坐标系：不随时间变化
  50ms);
```

📄 tf2 先在 3 秒前把领航者变到 `map`，在 `map` 里跨越时间，再在现在把它变到跟随者坐标系。

**用 3 参数版本** `lookupTransform("follower/base_link", "leader/base_link", now - 3s)`：问的是"3 秒前的领航者相对 **3 秒前的跟随者**"。📄 跟随者按自己 3 秒前的位置计算目标，现在执行，会走错方向、绕圈（文档里 turtle2 "uncontrollably" 乱跑）。

补充（💡）：启动后的前 3 秒 buffer 里还没有 3 秒前的数据，会报"外推到过去"，要容忍；查询时刻不要超过缓存长度（默认 10 s）。
