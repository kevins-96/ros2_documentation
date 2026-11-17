# 练习：测试与代码规范检查

> 适用：ROS 2 **Rolling**。对应卡片：[测试](../cards/engineering/testing.md)
> 代码：[test_path_math.cpp](build_lab/path_smoother_core/test/test_path_math.cpp)、[test_moving_average.cpp](build_lab/path_smoother_plugins/test/test_moving_average.cpp)、[test_smoother_node.py](build_lab/path_smoother_plugins/test/test_smoother_node.py)、[path_smoother_core/CMakeLists.txt](build_lab/path_smoother_core/CMakeLists.txt)（`INJECT_BUG` 选项和 lint 设置）
> 用时：约 30 分钟。在[编译与依赖的常见错误](build-pitfalls.md)第 0 部分建好的 `~/bl_ref` 里做

## 1. 全部测试

```bash
cd ~/bl_ref                       # 新终端，只 source /opt/ros/rolling/setup.bash
colcon build
colcon test
colcon test-result --all
```

先预测：一共有哪几类测试？哪些可能失败或被跳过？

<details>
<summary>参考结果（填完预测再展开）</summary>

2026-11-17 在 `ros:rolling-ros-base` 容器（**无法访问外网**）里实测：

```text
Summary: 28 tests, 0 errors, 2 failures, 5 skipped
```

| 来源 | 结果 |
|---|---|
| `test_path_math`（4 个 gtest 用例） | 通过 |
| `test_moving_average`（2 个，带 rclcpp 夹具） | 通过 |
| `test_smoother_node.py`（1 个活动测试 + 1 个关闭后测试） | 通过 |
| uncrustify、lint_cmake、flake8、pep257 | 通过 |
| cppcheck | **5 skipped**：`cppcheck 2.19.0 has known performance issues and therefore will not be used` |
| xmllint | **失败**：下载 `http://download.ros.org/schema/package_format3.xsd` 失败，判定 `package.xml` invalid。2 个 failures 就是它（ctest 层面 1 个 + xunit 层面 1 个） |
| copyright、cpplint | 被 `set(ament_cmake_*_FOUND TRUE)` 跳过，不出现 |

你的机器能上网时 xmllint 应该通过；cppcheck 是否跳过取决于你的 cppcheck 版本。

</details>

## 2. 让单元测试失败

```bash
colcon build --packages-select path_smoother_core --cmake-args -DINJECT_BUG=ON
colcon test --packages-select path_smoother_core; echo "colcon test 退出码: $?"
colcon test-result; echo "test-result 退出码: $?"
colcon test-result --verbose | head -40
```

| # | 问题 | 预测 | 实测 |
|---|---|---|---|
| 1 | 哪个用例失败？失败信息里有哪些数字？ | | |
| 2 | `colcon test` 的退出码 | | |
| 3 | `colcon test-result` 的退出码 | | |

再试：

```bash
colcon test --packages-select path_smoother_core --return-code-on-test-failure; echo $?
colcon test --packages-select path_smoother_core --ctest-args -R test_path_math
./build/path_smoother_core/test_path_math --gtest_filter='PathMath.MovingAverage*'
colcon build --packages-select path_smoother_core          # 不带 -D，bug 还在吗？
grep INJECT_BUG build/path_smoother_core/CMakeCache.txt
```

<details>
<summary>参考结果（填完预测再展开）</summary>

1. `PathMath.MovingAverageKeepsEndpoints` 失败：
   ```text
   test_path_math.cpp:35: Failure
   Expected equality of these values:
     out.back().x
       Which is: 9
     raw.back().x
       Which is: 10
   ```
   `out.back().y` 是 `0.33333333333333331`、期望 0。另外三个用例通过：`ASSERT` / `EXPECT` 只影响本用例。
2. **0**。`colcon test` 只报告"有测试失败"，退出码不变。
3. **1**。
4. 加 `--return-code-on-test-failure` 后退出码 1。
5. 不带 `-D` 重编，bug **还在**：`CMakeCache.txt` 里仍是 `INJECT_BUG:BOOL=ON`。用 `-DINJECT_BUG=OFF` 关掉。

</details>

## 3. 读到旧结果

```bash
# 接上一步（bug 还开着，test_path_math 有失败记录）
colcon test --packages-select path_smoother_core --ctest-args -R nothing_matches
colcon test-result --all | head -3
colcon test-result --delete-yes
colcon test-result --all | tail -1
```

预测第二条命令显示的 `test_path_math.gtest.xml` 结果。

<details>
<summary>参考结果（填完预测再展开）</summary>

ctest 打印 `No tests were found!!!`，但汇总里 `Test.xml: 0 tests`，同时**仍有** `test_path_math.gtest.xml: 4 tests, 0 errors, 1 failure`：那是上一次留下的文件。`--delete-yes` 之后汇总变成 `0 tests`。

同理，删掉某个测试（或从 CMake 里关掉某个 linter）后，它的旧 XML 会一直留在 `build/` 里被统计。

</details>

## 4. 集成测试

```bash
colcon build --packages-select path_smoother_core --cmake-args -DINJECT_BUG=OFF
colcon test --packages-select path_smoother_plugins --event-handlers console_direct+ 2>&1 | grep -E "run_rmw_isolated|plugin=|ok$|FAIL"
```

然后把 [test_smoother_node.py](build_lab/path_smoother_plugins/test/test_smoother_node.py) 复制一份，分别把参数 `'smoother_plugin': 'moving_average'` 改成 `'path_smoother_plugins::NoopSmoother'` 和 `'bogus'`，直接运行：

```bash
source install/setup.bash
python3 -m launch_testing.launch_test /tmp/t_noop.py
python3 -m launch_testing.launch_test /tmp/t_bogus.py
```

| # | 改成 | 预测：活动测试？关闭后测试？ | 实测 |
|---|---|---|---|
| 1 | `NoopSmoother` | | |
| 2 | `bogus` | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

- 正常版本：命令被包在 `/opt/ros/rolling/lib/rmw_test_fixture_implementation/run_rmw_isolated` 里执行（`RUNNER run_test_isolated.py` + `EnableRmwIsolation()` 的效果），两个测试都 `ok`。
- `NoopSmoother`：活动测试失败，`AssertionError: False is not true : [0.0, 1.0, 0.0, 1.0, ...]`，断言的第二个参数把实际数据打印了出来，一眼看出"没平滑"；关闭后测试 `ok`（节点被正常停止，退出码 0）。
- `bogus`：节点以退出码 1 退出，launch_testing 打印 `Processes under test stopped before tests completed` 和 `Process 'smoother_node-1' exited with 1`，再打印该进程的全部输出（含 `FATAL ... cannot load 'bogus'`），整个测试失败。

</details>

## 5. 代码规范

```bash
cd ~/bl_ref/src
ament_cpplint path_smoother_core path_smoother_plugins | tail -4
ament_copyright path_smoother_core | tail -2
ament_uncrustify path_smoother_core path_smoother_plugins | tail -2
AMENT_CPPCHECK_ALLOW_SLOW_VERSIONS=1 ament_cppcheck path_smoother_core | tail -2
```

然后在 [path_smoother_core/CMakeLists.txt](build_lab/path_smoother_core/CMakeLists.txt) 里注释掉 `set(ament_cmake_copyright_FOUND TRUE)` 这一行（只这一行），重新 build、test，看汇总里多了什么。

<details>
<summary>参考结果（填完预测再展开）</summary>

- cpplint：`Total errors found: 9`，全部是 `legal/copyright`（没有版权声明；作者最初还有 1 个超过 100 字符的行，已改掉）。所以模板要同时跳过 copyright 和 cpplint，只跳过一个不够。
- copyright：每个源文件都 `could not find copyright notice`。📄 可以用 `ament_copyright --add-missing "你的名字" apache2` 自动补上。
- uncrustify：`No problems found`（练习代码按 ROS 风格写的）。不合格时用 `ament_uncrustify --reformat` 自动改。
- cppcheck：设了环境变量后 `No problems found`（在 `colcon test` 里则是 `cppcheck.xunit.xml: 5 tests, 0 failures`）。
- 只取消跳过 copyright：汇总里多出 `copyright.xunit.xml: 5 tests, 5 failures`。改完记得恢复。

</details>

## 挑战（选做）

1. 给 `path_math` 的 `max_turn_angle` 写一个测试：直线路径为 0、直角拐弯为 $\pi/2$、180° 折返为 $\pi$。用 `EXPECT_NEAR` 而不是 `EXPECT_DOUBLE_EQ`，想一想为什么。
2. 在 `test_smoother_node.py` 里改用 `launch_testing_ros.WaitForTopics` 等待 `smoothed_path`（需要注意它的订阅 QoS 是否能收到 transient local 的消息）。
3. 写一个 gmock 测试：给 `Smoother` 做一个 mock 类，验证某个调用方对 `smooth()` 恰好调用一次。
