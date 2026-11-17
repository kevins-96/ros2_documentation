# 答案：测试

> 先在[卡片](../cards/engineering/testing.md)里自己写下答案再来对照。

## 题 1：CI 显示 PASSED，本地显示 0 failures

**CI 为什么 PASSED：** 🔍 `colcon test` 只要测试**跑完了**就返回 0，测试失败不影响退出码（实测注入 bug 后 `colcon test` 退出码 0，`colcon test-result` 退出码 1）。所以 `&& echo PASSED` 照样执行。

CI 的正确写法（二选一，都实测过）：

```bash
colcon build
colcon test --return-code-on-test-failure      # 有失败时退出码为 1
# 或者
colcon test
colcon test-result --verbose                   # 有失败时退出码为 1，并打印失败详情
```

💡 第二种更常见：`--verbose` 把失败输出直接打到 CI 日志里。

**本地为什么 0 failures：** 🔍 `colcon test-result` 汇总的是 `build/` 下**所有**现存的 XML，不区分是不是这次产生的。`--ctest-args -R some_test` 只运行并改写 `some_test` 的 XML；被改坏的另一个测试这次没跑，它的 XML 还是上一次通过时的结果，于是汇总显示 0 failures。（实测的反向情况：`-R` 匹配不到任何测试时，汇总仍显示上一次的失败。）

本地的正确做法：

1. 只想快速看一个测试时，用 `-R` 没问题，但**提交前完整跑一次**：`colcon test --packages-select 你的包`；
2. 每次完整跑之前先 `colcon test-result --delete-yes` 清掉旧 XML，或者只看 `colcon test` 本次的控制台输出；
3. 改了测试代码或被测代码，先 `colcon build` 再测（🔍 `colcon test` 用的是 `build/` 里已编译的测试程序）。

## 题 2：局部规划器的测试设计

**单元测试（gtest，不启动 ROS 通信）** 💡 把 `compute_velocity` 放进一个不依赖 ROS 通信的库：

| 用例 | 检查 |
|---|---|
| 机器人正对直线路径 | 角速度约为 0，线速度为期望值 |
| 路径在左/右侧 | 角速度符号正确 |
| 到达终点附近 | 线速度降到 0，不越过终点 |
| 空路径、只有一个点的路径 | 不崩溃，返回停止指令或抛出约定的异常 |
| 参数边界（最大速度、最大加速度） | 输出被限幅 |
| 每修一个 bug | 📄 先写一个能复现它的用例，再修 |

参数读取、插件 `configure` 这类需要 `rclcpp::Node` 但不需要通信的逻辑，用 gtest 夹具在 `SetUp` 里 `rclcpp::init`、`TearDown` 里 `shutdown`，用 `NodeOptions().append_parameter_override()` 注入参数。

**集成测试（launch_testing）**：启动节点，测试代码发布一条 `/plan` 和若干 `/odom`，检查 `/cmd_vel`：方向正确、到终点后为 0；`post_shutdown_test` 里 `assertExitCodes` 确认正常退出。集成测试少而精，只验证"接线"是否正确（话题名、QoS、坐标系），算法细节交给单元测试。

**两个问题的解法：**

1. **订阅者晚于节点、错过消息：**
   - 📄 用 `ReadyToTest()` 让测试在 launch 完成后才开始，而不是写 `sleep`；
   - 🔍 练习里 `smoother_node` 只发布一次，所以发布者和测试订阅者都用 **transient local** QoS，晚加入的订阅者也能收到最后一条；
   - 📄 用 `launch_testing_ros.WaitForTopics` 或在测试里循环 `spin_once` 直到收到消息或超时，而不是假设"一启动就有"；
   - 💡 本题中测试代码是 `/plan` 和 `/odom` 的发布者，可以先等节点的订阅者出现（`count_subscribers() > 0`）再发。
2. **另一个测试也在发 `/odom`：** 📄 用 `EnableRmwIsolation()` 加 `run_test_isolated.py` 作为 `RUNNER`，每个测试运行在隔离的通信环境里；🔍 实测命令被包在 `run_rmw_isolated` 里执行。
