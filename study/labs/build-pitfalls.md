# 练习：编译与依赖的常见错误

> 适用：ROS 2 **Rolling**。对应卡片：[工作空间与 colcon](../cards/engineering/workspace-colcon.md)、[ament_cmake](../cards/engineering/ament-cmake.md)、[rosdep 依赖管理](../cards/engineering/rosdep.md)
> 代码：[build_lab/path_smoother_core](build_lab/path_smoother_core/CMakeLists.txt)、[build_lab/path_smoother_plugins](build_lab/path_smoother_plugins/CMakeLists.txt)
> 用时：约 40 分钟

这个练习的做法是：**在正确的包上每次只改一处**，干净编译，看错误出现在哪个包、是什么信息。改动都在副本里做，不碰仓库。

## 0. 准备

```bash
# 一个"正确版本"的工作空间，后面所有实验都从这里复制
mkdir -p ~/bl_ref/src && cp -r <你的仓库路径>/study/labs/build_lab/* ~/bl_ref/src/

# 每个实验用一个新目录：exp 名字 '要执行的修改'
exp() {
  rm -rf ~/bl_$1 && mkdir -p ~/bl_$1 && cp -r ~/bl_ref/src ~/bl_$1/
  (cd ~/bl_$1/src && eval "$2")
  (cd ~/bl_$1 && colcon build 2>&1 | tail -20)
}
```

**每个实验都在新终端里做，只 `source /opt/ros/rolling/setup.bash`**。不要在 source 过某个实验 `install/` 的终端里编下一个实验（原因见第 4 部分，这正是作者实测时踩到的坑）。

两个包的关系：

| 包 | 内容 |
|---|---|
| `path_smoother_core` | 纯 C++ 库 `path_math`（折线长度、最大转角、滑动平均）；插件基类 `Smoother`（只有头文件）；节点 `smoother_node`（按参数名加载插件）；`config/`、`launch/` |
| `path_smoother_plugins` | 两个插件：`moving_average`、`NoopSmoother`；依赖 core |

先编一次正确版本，确认能跑：

```bash
cd ~/bl_ref && colcon build && source install/setup.bash
colcon graph
ros2 launch path_smoother_core smoother.launch.xml
```

## 1. 安装和导出（ament_cmake）

| # | 修改 | 预测：哪个包失败？报什么？ | 实测 |
|---|---|---|---|
| A | `exp A "sed -i '/install(TARGETS smoother_node/d' path_smoother_core/CMakeLists.txt"`，然后 source 并 `ros2 run path_smoother_core smoother_node` | | |
| B | `exp B "sed -i '/^ament_export_targets/d' path_smoother_core/CMakeLists.txt"` | | |
| C | `exp C "sed -i '/install(DIRECTORY include/d' path_smoother_core/CMakeLists.txt"` | | |
| D | `exp D "sed -i 's/^project(path_smoother_core)/project(path_smoother)/' path_smoother_core/CMakeLists.txt"` | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

2026-11-17 在 `ros:rolling-ros-base` 里实测：

| # | 结果 |
|---|---|
| A | 两个包都**编译成功**。`ros2 run` 输出 `No executable found`；`ros2 pkg executables path_smoother_core` 什么都不打印 |
| B | core 成功；**plugins 失败**：`Target "path_smoother_plugins" links to: path_smoother_core::smoother_interface but the target was not found` |
| C | core 成功；**plugins 失败**：`Imported target "path_smoother_core::smoother_interface" includes non-existent path "/.../install/path_smoother_core/include/path_smoother_core"` |
| D | **core 失败**，错误在最后的 `ament_package()`：`ament_package_xml() package name 'path_smoother_core' in '/package.xml' does not match current PROJECT_NAME 'path_smoother'`；汇总里 plugins 是 `not processed` |

规律：安装和导出写错，**上游自己几乎都能编过**，错误在下游或运行时才出现。

</details>

## 2. 依赖声明（colcon 排序）

```bash
exp E "sed -i '/<depend>path_smoother_core/d' path_smoother_plugins/package.xml"
cd ~/bl_E && colcon graph
```

预测：
1. `colcon graph` 和正确版本相比有什么不同？
2. 干净编译的结果？
3. 接着执行 `colcon build --packages-select path_smoother_core`，再 `colcon build`（**不 source**），结果？
4. 再 `source install/setup.bash` 后 `colcon build`，结果？

<details>
<summary>参考结果（填完预测再展开）</summary>

1. 两个包之间不再有 `*`：colcon 认为它们互不相关。
2. 两个包**同时开始**，plugins 报 `Could not find a package configuration file provided by "path_smoother_core"`，core 随即 `Aborted`。
3. 仍然失败，同样的错误。colcon 只把**声明了的依赖**加进一个包的编译环境，core 虽然已经装好，plugins 也看不到它。
4. 成功。因为 source 之后 `CMAKE_PREFIX_PATH` 里有 core。这就是"我本地能编过"的来源，换台机器就失败。

</details>

## 3. symlink-install 和 local_setup

```bash
mkdir -p ~/bl_sl/src && cp -r ~/bl_ref/src/* ~/bl_sl/src/ && cd ~/bl_sl
colcon build --symlink-install
ls -l install/path_smoother_core/share/path_smoother_core/config/
ls -l install/path_smoother_core/lib/path_smoother_core/
source install/setup.bash
sed -i 's/window: 5/window: 7/' src/path_smoother_core/config/smoother.yaml
ros2 launch path_smoother_core smoother.launch.xml       # 看 window=?
```

| # | 问题 | 预测 | 实测 |
|---|---|---|---|
| 1 | `install/` 里的 YAML 和可执行文件分别是什么？ | | |
| 2 | 改 YAML 后不重编，launch 读到的 `window` 是几？不加 `--symlink-install` 又是几？ | | |
| 3 | 再执行一次**不带** `--symlink-install` 的 `colcon build`，YAML 还是链接吗？ | | |
| 4 | 新终端里只 `source ~/bl_sl/install/local_setup.bash`，`which ros2` 输出什么？ | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

1. YAML 是指向 `src/.../config/smoother.yaml` 的符号链接；可执行文件是指向 `build/path_smoother_core/smoother_node` 的符号链接；头文件也链接到 `src/`。
2. 加了：`window=7`；没加：仍是 `window=5`（`install/` 里是编译时复制的旧文件）。改 `.cpp` 不管加不加都要重编。
3. **仍是链接**。关掉选项不会把链接变回文件，切换方式时删掉 `build/` 和 `install/`。
4. 找不到 `ros2`。`AMENT_PREFIX_PATH` 里只有本工作空间的两个包，没有 `/opt/ros/rolling`。

</details>

## 4. 工作空间链（underlay 被写死）

```bash
# 终端 1
source /opt/ros/rolling/setup.bash
mkdir -p ~/ws_a/src ~/ws_b/src
cp -r ~/bl_ref/src/path_smoother_core ~/ws_a/src/
cp -r ~/bl_ref/src/path_smoother_plugins ~/ws_b/src/
cd ~/ws_a && colcon build && source install/setup.bash
cd ~/ws_b && colcon build                       # plugins 用 ws_a 里的 core
grep -n "ws_a" ~/ws_b/install/setup.sh
rm -rf ~/ws_a
# 终端 2（新开）
source ~/ws_b/install/setup.bash; echo "rc=$?"
ros2 run path_smoother_core smoother_node
```

预测：`source` 报错吗？返回码？`ros2 run` 的结果？

<details>
<summary>参考结果（填完预测再展开）</summary>

- `ws_b/install/setup.sh` 里写死了 `COLCON_CURRENT_PREFIX="/.../ws_a/install"`：编译 `ws_b` 时 source 了哪些工作空间，`ws_b` 的 `setup.bash` 就会永远去 source 它们。
- 删除 `ws_a` 后，`source` 只打印一行 `not found: ".../ws_a/install/local_setup.bash"`，**返回码 0**。
- `ros2 run` 报 `Package 'path_smoother_core' not found`。

作者实测时的真实教训：用同一个 shell 依次编译多个实验工作空间并 source，后面的工作空间全部链上了前面的，导致某个实验"删掉的配置居然还生效"，看起来像 pluginlib 的怪行为。换成每个实验一个干净 shell 后结果才正确。

</details>

## 5. overlay 覆盖 underlay

```bash
# 新终端
source /opt/ros/rolling/setup.bash && source ~/bl_ref/install/setup.bash
mkdir -p ~/bl_over/src && cp -r ~/bl_ref/src/path_smoother_core ~/bl_over/src/
sed -i 's/plugin=%s points/[OVERLAY] plugin=%s points/' ~/bl_over/src/path_smoother_core/src/smoother_node.cpp
cd ~/bl_over && colcon build
# 再开新终端
source /opt/ros/rolling/setup.bash && source ~/bl_over/install/setup.bash
ros2 pkg prefix path_smoother_core
ros2 run path_smoother_core smoother_node        # Ctrl-C 结束
```

预测：编译时有没有警告？运行的是哪个版本？此时 plugins 用的是哪个 core 的头文件编译的？

<details>
<summary>参考结果（填完预测再展开）</summary>

- 实测镜像（colcon-core 0.21.3）里**没有任何警告**，`colcon build --allow-overriding` 报 `unrecognized arguments`。💡 覆盖警告来自扩展 `colcon-override-check`，你的机器上如果装了，会提示用 `--allow-overriding path_smoother_core`。
- `ros2 pkg prefix` 指向 `~/bl_over/install/path_smoother_core`，日志里有 `[OVERLAY]`。
- plugins 仍在 `bl_ref` 里，是对着 `bl_ref` 的 core 头文件编译的。这次只改了日志字符串所以没事；如果改了 `Smoother` 的虚函数，plugins 就会在运行时出错。改 underlay 的包时，把依赖它的包一起拉进 overlay 重编。

</details>

## 6. rosdep（需要联网）

```bash
cd ~/bl_ref
rosdep keys --from-paths src --ignore-src          # 先预测输出
rosdep keys --from-paths src                       # 对比
rosdep check --from-paths src --ignore-src -r

# 加一个未知依赖
sed -i 's|<depend>pluginlib</depend>|<depend>pluginlib</depend>\n  <depend>my_motor_sdk</depend>|' \
  src/path_smoother_plugins/package.xml
rosdep install --from-paths src --ignore-src -y --simulate

# 自定义规则
sudo mkdir -p /etc/ros/rosdep && printf 'my_motor_sdk:\n  ubuntu: [libsocketcan-dev]\n' | \
  sudo tee /etc/ros/rosdep/custom_rules.yaml
echo "yaml file:///etc/ros/rosdep/custom_rules.yaml" | sudo tee /etc/ros/rosdep/sources.list.d/30-custom.list
rosdep resolve my_motor_sdk                        # update 之前
rosdep update
rosdep resolve my_motor_sdk                        # update 之后
rosdep where-defined my_motor_sdk
```

做完后**撤销**：删掉 `30-custom.list` 和 `custom_rules.yaml`，`rosdep update`，把 `package.xml` 改回来。

<details>
<summary>参考结果（填完预测再展开）</summary>

实测（官方镜像，把代理 CA 挂进容器后联网）：

- 带 `--ignore-src` 的 `rosdep keys` **输出为空**；不带时列出 13 个 key（`ament_cmake`、`rclcpp`、`nav_msgs`、`pluginlib`、`path_smoother_core` 等）。原因：环境里 `AMENT_PREFIX_PATH=/opt/ros/rolling`，`--ignore-src` 把 underlay 里已有的包也当作"有源码"，不用装。
- 未知 key：`Cannot locate rosdep definition for [my_motor_sdk]`。
- 自定义规则：update 之前 `ERROR: no rosdep rule for 'my_motor_sdk'`；update 时出现 `Hit file:///.../custom_rules.yaml`；之后解析为 `libsocketcan-dev`，`where-defined` 指向你的文件。
- 选做：在同一个文件里加 `python3-numpy: {ubuntu: [my-patched-numpy]}`。放在 `30-` 文件里不生效；改名为 `10-custom.list` 并 update 后生效。体会一下文档为什么警告"不要随意覆盖已有 key"。

</details>

## 7. 已经不存在的宏

```bash
exp F "sed -i 's/^target_link_libraries(smoother_node$/ament_target_dependencies(smoother_node rclcpp)\ntarget_link_libraries(smoother_node/' path_smoother_core/CMakeLists.txt"
```

预测：Rolling 上会怎样？

<details>
<summary>参考结果（填完预测再展开）</summary>

core 失败：`Unknown CMake command "ament_target_dependencies"`。它在 Kilted 弃用，Rolling 已删除。见 [ament_cmake](../cards/engineering/ament-cmake.md) 自测题 2。

</details>

## 挑战（选做）

1. 在 `path_smoother_core` 里给 `path_math` 改成 `add_library(path_math STATIC ...)`，再看 plugins 能不能编过、`ldd install/path_smoother_plugins/lib/libpath_smoother_plugins.so` 里还有没有 `libpath_math.so`。
2. 用 `--cmake-args -DCMAKE_BUILD_TYPE=Release` 编一次，再不带参数编一次，查 `build/path_smoother_core/CMakeCache.txt` 里的 `CMAKE_BUILD_TYPE`。
3. 给 `path_smoother_core` 写一个只有 `exec_depend` 的"变体"元包 `smoother_all`，依赖 core 和 plugins。
