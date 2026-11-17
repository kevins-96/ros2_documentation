# 答案：日志

> 先在[卡片](../cards/configuration/logging.md)里自己写下答案再来对照。

## 题 1：只看 MPC 求解器的 DEBUG

```bash
--ros-args --log-level robot3.controller.mpc:=debug
```

要点：
1. logger 名用 `.` 分隔，并且包含命名空间。🔍 实测在 `/robot1` 下，`battery_monitor:=debug` 得到 0 条 DEBUG，`robot1.battery_monitor:=debug` 得到 14 条。所以这里写 `controller.mpc:=debug` 不生效。
2. 只设子 logger，父 logger `robot3.controller` 仍是默认的 INFO，控制器其他部分的 DEBUG 不会出现。📄 层级继承是"子跟父"，不是"父跟子"。
3. 不要加全局的 `--log-level debug`。🔍 实测全局 DEBUG 时 1.2 s 内 `rcl` 打了 75 条、`rmw_fastrtps_cpp` 36 条，会淹没你要看的内容。

💡 子 logger 的名字由代码决定：`get_logger().get_child("mpc")` 得到的就是 `robot3.controller.mpc`。拿不准时，用 `RCUTILS_CONSOLE_OUTPUT_FORMAT="[{severity}] [{name}] {message}"` 把 logger 名打出来看一眼。

## 题 2：50 Hz 的偏离告警刷屏

现在的写法相当于每个控制周期判断一次，偏离期间每秒 50 条。🔍 卡片坑 1 实测：`_EXPRESSION` 不节流，条件为真的每个周期都打。

**改法 A：节流**

```cpp
RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 1000,
  "off path %.2f m", deviation);
```

- 好处：改动最小；持续偏离时每秒还有一条，能看到偏离量的变化。
- 代价：1 秒内的短暂偏离可能只打一条或者恰好被吞掉；仿真时间暂停时节流不结束（🔍 THROTTLE 用节点时钟）。

**改法 B：只在状态变化时打**

```cpp
const bool off = deviation > 0.3;
if (off && !was_off_) {
  RCLCPP_WARN(get_logger(), "left path, deviation %.2f m", deviation);
} else if (!off && was_off_) {
  RCLCPP_INFO(get_logger(), "back on path");
}
was_off_ = off;
```

- 好处：每次偏离只有"开始"和"结束"两条，日志最干净，还能算出偏离持续了多久。
- 代价：要多维护一个状态变量；偏离期间偏离量变大还是变小看不到。💡 在 0.3 m 附近抖动时会反复打，可以加滞回（hysteresis），比如超过 0.3 m 进入、低于 0.25 m 才退出。

💡 实际项目里常把两者结合：状态变化时打 WARN，偏离期间用 THROTTLE 每几秒补一条当前偏离量。高频的数值本身更适合发布成话题，用 rqt_plot 或录 bag 分析，而不是写进日志。
