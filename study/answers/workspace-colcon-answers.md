# 答案：工作空间与 colcon

> 先在[卡片](../cards/engineering/workspace-colcon.md)里自己写下答案再来对照。

## 题 1：删掉 underlay 之后

**原因：** `my_ws` 是在 source 了 `nav2_ws` 的终端里编译的，colcon 把当时的 underlay 链（`/opt/ros/rolling` 和 `~/nav2_ws/install`）写进了 `my_ws/install/setup.sh`。`source ~/my_ws/install/setup.bash` 会依次 source 这条链。`nav2_ws` 被删后：

- 🔍 setup 脚本只打印一行 `not found: ".../install/local_setup.bash"`，**返回码仍是 0**，很容易被忽略；
- 🔍 `AMENT_PREFIX_PATH` 里只剩 `my_ws` 和 `/opt/ros/rolling`，原来由 `nav2_ws` 提供的包自然找不到（实测为 `Package 'path_smoother_core' not found`）。

注意 `nav2_controller` 本来就可能被 apt 装在 `/opt/ros/rolling` 里；如果找不到，说明你只在 `nav2_ws` 里有它。

**以后怎么做：** 📄 编译和 source 分开在不同终端；💡 每个工作空间的 underlay 关系要**有意识地**建立：

1. 新终端只 `source /opt/ros/rolling/setup.bash`，编译 `nav2_ws`；
2. 再开新终端，`source /opt/ros/rolling/setup.bash && source ~/nav2_ws/install/setup.bash`，然后编译 `my_ws`；
3. 想改变 underlay（比如删掉 `nav2_ws`），就在只 source 了新 underlay 的终端里**删掉 `my_ws` 的 `build/` 和 `install/` 重编**，而不是只删 underlay。

💡 可以在 `~/.bashrc` 里只写 `source /opt/ros/rolling/setup.bash`，不要写 overlay，免得每个终端都"自动"成了编译用的 underlay。

## 题 2：漏写依赖

**(a) CI 为什么失败：** 🔍 colcon 只根据 `package.xml` 排顺序。没写依赖，两个包被认为互不相关，干净编译时**同时开始**，`my_planner` 的 `find_package(my_costmap_utils)` 执行时对方还没装好，报 `Could not find a package configuration file provided by "my_costmap_utils"`。

**(b) 本地为什么能编过：** 可能的原因有：

- 🔍 他之前已经编好了 `my_costmap_utils`，并且在这个终端里 `source` 过 `install/setup.bash`，`CMAKE_PREFIX_PATH` 里已经有它。实测：只编好 core 但不 source，再 `colcon build` 仍失败；source 之后才成功。这是因为 colcon 只为一个包准备它**声明了的**依赖的环境。
- 💡 他用 `--packages-select my_planner` 单独编，前面编过的结果恰好在环境里。
- 💡 调度"碰巧"先把 `my_costmap_utils` 编完了（比如 `--executor sequential` 且字母序在前）。

**为什么不可靠：** 这些都依赖"上一次编译留下的状态"或调度顺序，换台机器、清空 `install/`、多核并行就会失败。另外 💡 漏写依赖还会让 `rosdep install` 漏装、二进制发布时缺包。规则：**`find_package` 了什么，`package.xml` 里就写什么**。
