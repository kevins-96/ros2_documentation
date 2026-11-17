# 练习：用插件换路径平滑器

> 适用：ROS 2 **Rolling**。对应卡片：[pluginlib 插件](../cards/engineering/pluginlib.md)
> 代码：[smoother.hpp](build_lab/path_smoother_core/include/path_smoother_core/smoother.hpp)、[smoother_node.cpp](build_lab/path_smoother_core/src/smoother_node.cpp)、[moving_average_smoother.cpp](build_lab/path_smoother_plugins/src/moving_average_smoother.cpp)、[noop_smoother.cpp](build_lab/path_smoother_plugins/src/noop_smoother.cpp)、[plugins.xml](build_lab/path_smoother_plugins/plugins.xml)
> 用时：约 30 分钟。需要先做完[编译与依赖的常见错误](build-pitfalls.md)第 0 部分（`~/bl_ref` 和 `exp` 函数）

`smoother_node` 生成一条锯齿路径（x 每步 +1，y 在 0 和 1 之间跳），按参数 `smoother_plugin` 的名字加载平滑器，打印平滑前后的长度和最大转角，并用 transient local QoS 在 `smoothed_path` 上发布一次 `nav_msgs/Path`。这和 Nav2 `smoother_server` 按名字加载平滑器插件是同一个模式。

## 1. 正常加载

```bash
cd ~/bl_ref && source install/setup.bash
ros2 plugin list | grep -A3 '^path_smoother'
ros2 run path_smoother_core smoother_node --ros-args -p smoother.window:=5
ros2 run path_smoother_core smoother_node --ros-args -p smoother_plugin:=path_smoother_plugins::NoopSmoother
```

| # | 运行 | 预测：能加载吗？长度和最大转角 | 实测 |
|---|---|---|---|
| 1 | `smoother.window:=5`（默认插件 `moving_average`） | | |
| 2 | `smoother_plugin:=path_smoother_plugins::NoopSmoother` | | |
| 3 | `smoother_plugin:=path_smoother_plugins::MovingAverageSmoother` | | |
| 4 | `smoother.window:=4` | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

```text
path_smoother_plugins:
	moving_average [path_smoother_plugins::MovingAverageSmoother] (base: path_smoother_core::Smoother)
	path_smoother_plugins::NoopSmoother [path_smoother_plugins::NoopSmoother] (base: path_smoother_core::Smoother)
```

| # | 结果 |
|---|---|
| 1 | `window=5`；`length 14.142 -> 10.231, max turn 90.0 -> 22.6 deg` |
| 2 | 能加载（XML 里没写 `name`，查找名就是 `type`）；`14.142 -> 14.142, 90.0 -> 90.0` |
| 3 | **失败**：`the class path_smoother_plugins::MovingAverageSmoother with base class type path_smoother_core::Smoother does not exist. Declared types are  moving_average path_smoother_plugins::NoopSmoother`。写了 `name` 后只能用 `name` 查找，和文档"type 和 name 都可以"的说法不符 |
| 4 | 插件 `configure()` 抛 `std::invalid_argument: smoother.window must be a positive odd number`，节点没捕获，`Aborted` |

几何直觉：锯齿路径每一段长 $\sqrt{2}$，10 段共 $10\sqrt{2}\approx 14.142$，每个拐点转 90°。滑动平均把 y 拉向 0.5，路径变短、转角变小；窗口越大越平滑，但也越偏离原路径（拐角被"切"掉）。

</details>

## 2. 五种"编译通过、加载失败"

每个实验在新终端里做：`exp` 编好之后，新开终端 `source /opt/ros/rolling/setup.bash && source ~/bl_X/install/setup.bash`，再执行 `ros2 plugin list | grep -A3 '^path_smoother'` 和下面两条命令：

```bash
ros2 run path_smoother_core smoother_node --ros-args -p smoother_plugin:=moving_average
ros2 run path_smoother_core smoother_node --ros-args -p smoother_plugin:=path_smoother_plugins::NoopSmoother
```

| # | 修改 | 预测：`plugin list` 正常吗？哪个能加载？报什么？ | 实测 |
|---|---|---|---|
| P1 | `exp P1 "sed -i '0,/path_smoother_core::Smoother/s//path_smoother_core::Smootherr/' path_smoother_plugins/plugins.xml"` | | |
| P2 | `exp P2 "sed -i '/PLUGINLIB_EXPORT_CLASS/d' path_smoother_plugins/src/noop_smoother.cpp"` | | |
| P3 | `exp P3 "sed -i '/^pluginlib_export_plugin_description_file/d' path_smoother_plugins/CMakeLists.txt"` | | |
| P4 | `exp P4 "sed -i 's/^pluginlib_export_plugin_description_file(path_smoother_core/pluginlib_export_plugin_description_file(path_smoother_plugins/' path_smoother_plugins/CMakeLists.txt"` | | |
| P5 | `exp P5 "sed -i 's/library path=.path_smoother_plugins./library path=\"libpath_smoother_plugins.so\"/' path_smoother_plugins/plugins.xml"` | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

2026-11-17 实测，pluginlib 6.0.3。五个实验**都编译成功**：

| # | `ros2 plugin list` | `moving_average` | `NoopSmoother` |
|---|---|---|---|
| P1 基类名写错 | 正常列出，但显示 `(base: path_smoother_core::Smootherr)` | `does not exist. Declared types are  path_smoother_plugins::NoopSmoother` | 正常 |
| P2 缺导出宏 | **正常列出两个** | 正常 | `Could not create object of class type path_smoother_plugins::NoopSmoother as no factory exists for it` |
| P3 没登记 XML | 没有 `path_smoother_plugins` 这一组 | `does not exist. Declared types are`（空） | 同左 |
| P4 登记到了本包名下 | **正常列出两个** | `does not exist. Declared types are`（空） | 同左 |
| P5 library path 写成文件名 | 正常列出两个 | `Could not find library corresponding to plugin moving_average. Make sure that the library 'libpath_smoother_plugins.so' actually exists.` | 同左 |

要点：
- `ros2 plugin list` 只读 XML，P2、P4、P5 列表都正常，**不能用它证明插件能加载**。
- P4 最迷惑。原因在 ament 索引：`ls ~/bl_P4/install/path_smoother_plugins/share/ament_index/resource_index/` 会看到 `path_smoother_plugins__pluginlib__plugin`，而 `ClassLoader("path_smoother_core", ...)` 找的是 `path_smoother_core__pluginlib__plugin`。
- 排查顺序：错误信息里 `Declared types are` 后面是否为空 → 空就查登记（P3、P4）和 `ClassLoader` 的两个参数；不空但没有你的插件 → 查 `base_class_type`（P1）和查找名；`no factory` → 查导出宏（P2）；`Could not find library` → 查 `library path` 和库是否装了（P5）。

</details>

## 3. ClassLoader 和插件的销毁顺序

阅读 [smoother_node.cpp](build_lab/path_smoother_core/src/smoother_node.cpp) 末尾的 `smoother.reset()`。如果在插件对象还活着时先销毁 `ClassLoader`（例如把 loader 放进 `std::unique_ptr` 并先 `reset()`），会发生什么？

<details>
<summary>参考结果（填完预测再展开）</summary>

🔍 用一个小程序实测：先 `loader.reset()`，再用插件、再销毁插件，打印

```text
Warning: class_loader.ClassLoader: SEVERE WARNING!!! Attempting to unload library while objects created by this loader exist in the heap! You should delete your objects before attempting to unload the library or destroying the ClassLoader. The library will NOT be unloaded.
```

之后插件照常工作，程序正常退出：class_loader 发现还有对象，就**不卸载**库。不会崩溃不代表正确，类成员里应把 `ClassLoader` 声明在插件指针之前。

</details>

## 挑战（选做）

1. 写第三个插件 `ChaikinSmoother`（Chaikin 切角算法：每条边 $P_iP_{i+1}$ 换成 $\tfrac34P_i+\tfrac14P_{i+1}$ 和 $\tfrac14P_i+\tfrac34P_{i+1}$ 两个点，首尾点保留），参数 `smoother.iterations`。不改 `path_smoother_core` 的任何代码，只改插件包，让 `smoother_plugin:=chaikin` 能用。
2. 让 `smoother_node` 同时加载两个实例（`smoother_a`、`smoother_b`，不同窗口），体会为什么插件参数要带实例名前缀。
3. 把 `smoother_node` 里的 `try/catch` 扩大到 `configure()`，让第 1 部分的 #4 不再 `Aborted`，而是打印 FATAL 并以退出码 1 退出。
