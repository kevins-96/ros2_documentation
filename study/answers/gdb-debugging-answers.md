# 答案：调试与回溯

> 先在[卡片](../cards/observability/gdb-debugging.md)里自己写下答案再来对照。

## 题 1：Release 回溯第 0 帧是 `std::_Function_handler<...Odometry...>::_M_invoke`

**能读出什么：**

- 崩溃发生在一个**订阅 `Odometry` 的回调**里（模板参数里的 `std::shared_ptr<nav_msgs::msg::Odometry const>`），往下几帧应该能看到 `rclcpp::Subscription<...>::handle_message` / `Executor::execute_subscription`，可以确认是订阅回调而不是定时器。
- 第 0 帧是 `std::function` 的包装，说明你的回调 lambda 以及它调用的小函数都被**内联**进来了。🔍 练习里 Release 构建的 `count_waypoints`、`validate_plan` 就是这样消失的。真正出错的代码是"里程计回调以及它直接调用、被内联的函数"之一。
- 没有行号，是因为 Release 没有 `-g`。

**下一步：** 用 `RelWithDebInfo` 重新编译这个包（`-O2 -g`）。🔍 同一个崩溃，`RelWithDebInfo` 的第 0 帧能给出回调里**调用点那一行**，`info locals` 还能看到 `plan = 0x0`。优化级别从 `-O3` 变成 `-O2`，💡 对大多数规划代码性能差别很小；调试信息放在单独的段里，不参与运行。配合 `ulimit -c unlimited` 让现场崩溃时留下 core 文件（💡），回到桌面用 `gdb <可执行文件> <core>` 离线看回溯，就不需要在现场开 gdb。

## 题 2：加了 `-DCMAKE_BUILD_TYPE=Debug` 还是没有行号

1. **CMake 缓存里还是旧的构建类型，或者被包自己覆盖了。** 检查 `grep CMAKE_BUILD_TYPE build/<包>/CMakeCache.txt`。如果是旧值，用 `colcon build --packages-select <包> --cmake-clean-cache --cmake-args -DCMAKE_BUILD_TYPE=Debug`。再检查包的 `CMakeLists.txt` 有没有写死 `set(CMAKE_BUILD_TYPE Release)` 或者 `add_compile_options(-O3)` 之类。
2. **gdb 跑的不是刚编的那个可执行文件。** 比如新终端没有 source 这个工作空间，`ros2 run` 找到的是 underlay（`/opt/ros/...` 或另一个工作空间）里的同名包。检查 `ros2 pkg prefix <包>`，看路径是不是你的 `install/`。🔍 第 5 批的练习里也遇到过"在 source 了别的工作空间的终端里编译"带来的混乱。
3. **只给了一个包 Debug，崩溃在别的包里。** 回溯里别的库（比如你依赖的 `path_smoother_core`）的帧没有行号，因为那个包没用 Debug 编。看帧后面的 `from /path/lib/xxx.so`，对应的包也要带 Debug 重编。
4. **编了但没有安装或被 strip 了。** 💡 自定义的安装步骤里如果有 `strip`，或者打包时剥离了调试信息，安装后的文件就没有行号。用 `readelf -S <可执行文件> | grep debug` 看有没有 `.debug_info` 段。
