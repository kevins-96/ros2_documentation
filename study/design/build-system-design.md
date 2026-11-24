---
tags: [ros2, 设计理念, 构建, ament, colcon, 交叉编译]
distro: rolling
status: design-batch-11
aliases: [构建系统的设计, ament 的设计, colcon 的设计, build tool vs build system, 构建工具与构建系统, AMENT_PREFIX_PATH, 交叉编译的设计, cross compilation, sysroot]
---

# 构建系统的设计：ament、colcon 和交叉编译

> 适用：ROS 2 **Rolling**（2026-11-23 在 `ros:rolling-ros-base`、colcon-core 0.21.3、colcon-ros 0.5.0、ament_cmake 2.9.3、GCC 15 aarch64 交叉编译器上实测）
> 来源标记：📐 设计文档原文 · 📄 本仓库文档原文 · 🔍 源码/实测核实 · 💡 经验补充
> 设计文档（[ros2/design](https://github.com/ros2/design) @ `93a415b`，本地副本在 `external/ros2_design/articles/`）：
> [The build system "ament_cmake" and the meta build tool "ament_tools"](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/100_ament.md)（2015-07，2018-06 修订）、
> [A universal build tool](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/101_build_tool.md)（2017-03，2021-01 修订）、
> [Cross-compiling ROS 2 packages](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/cross_compilation_build_tools_design.md)（2019-09，草稿）
> 先读卡片 [工作空间与 colcon](../cards/engineering/workspace-colcon.md)、[ament_cmake](../cards/engineering/ament-cmake.md) 和练习 [编译与依赖的常见错误](../labs/build-pitfalls.md)。本篇讲"为什么这样设计"。

## 1. 设计要回答的问题

ROS 是联邦式开发的：几百个包分散在不同仓库、由不同的人维护 📐。手工构建一组互相依赖的包，要自己排拓扑顺序、读每个包的文档、一个个编译安装、每装一个就配一次环境 📐。设计要解决：

1. 怎么**一条命令**按依赖顺序构建一堆包，而且包可以用不同的构建系统（CMake、Python setuptools……）。
2. 构建完怎么**配好环境**，让下游包找得到、程序跑得起来。
3. 怎么不重蹈 catkin 的覆辙，并且和 ROS 1 并存。
4. 怎么给 ARM 这类目标平台交叉编译。

## 2. 考虑过的方案与取舍

### 构建工具 vs 构建系统（101）📐

| | 构建工具（build tool） | 构建系统（build system） |
|---|---|---|
| 操作对象 | **一组**包 | **一个**包 |
| 做什么 | 算依赖图，按拓扑序给每个包调用它的构建系统，配环境 | 配置、编译、安装这一个包 |
| 例子 | `catkin_make`、`catkin_make_isolated`、`catkin_tools`、`ament_tools`、**colcon** | CMake、Python setuptools、`catkin`、**`ament_cmake`** |

构建工具应该对构建系统**知道得越少越好**，够它配环境、调用构建、配置使用环境就行 📐。环境配置最好由构建系统自己提供，这样用户不用构建工具、手动编一个包时也能用 📐。

### ament 相对 catkin 改了什么（100）📐

| catkin 的问题 | ament 的做法 |
|---|---|
| 以 CMake 为中心，纯 Python 包也要走 CMake，`setup.py` 只能用 setuptools 的一小部分功能 | 支持多种构建类型：`ament_cmake`、`cmake`、`ament_python`，由 `package.xml` 的 `<build_type>` 决定 |
| **devel space**：不安装也能用，但逻辑复杂，每个包的 CMake 都要照顾它 | 去掉 devel space，改成可选的 **symlink install**（`--symlink-install`），CMake 包通过**覆盖 CMake 的 `install()`** 实现，Python 包用 setuptools 的开发模式 |
| 直接用 `CMAKE_PREFIX_PATH` 存多个工作空间的前缀，和别的用途冲突，而且是 CMake 专用变量 | 用独立的 `AMENT_PREFIX_PATH`（运行时用），构建时再从它推出 CMake 需要的变量 |
| `catkin_make` 把所有包放进**同一个 CMake 上下文**：目标名、函数、宏、测试全局唯一，还要声明跨包的目标依赖 | **不提供这个功能**：每个包单独配置、单独构建 |
| `catkin_simple` 受 catkin 设计限制（例如 `catkin_package()` 必须在目标之前调用），有些情况做不到 | `ament_cmake_auto` 从 `package.xml` 读依赖，自动处理 `include/`、`msg/`、安装位置 |
| 单体包：例如内置 gtest 后很难再支持 gmock | 高度模块化：几乎每个功能是一个单独的包，CMake 部分有扩展点 |

**为什么不直接改 catkin？** 不能影响 ROS 1 📐。**为什么换名字？** ROS 1 和 ROS 2 的桥要在同一个工程里同时找到两套构建系统，`find_package(catkin)` 分不清是哪个；叫 `catkin2` 也会让人混淆 📐。（ament 本身就是 catkin 的同义词，都是"柔荑花序"。）

### 环境怎么配（100）📐

- 每个包在 `<prefix>/share/<pkg>/` 下放自己的环境脚本。
- 安装目录根上的 `local_setup.*` **只**遍历本安装目录里的包（从 ament 索引里读包列表），依次 source 它们。
- `setup.*` 先 source 父工作空间（underlay），再 source `local_setup.*`。
- **ament 资源索引（resource index）**：构建时生成，运行时高效查询"有哪些包、哪些消息、哪些插件"。

### 为什么选 colcon（101）📐

101 列出现有工具的长短：`catkin_make` 单上下文、快但冲突多；`catkin_make_isolated` 隔离但串行；`catkin_tools` 并行、好用，但只支持 CMake；`ament_tools` 支持 Python 和无清单包、跨平台，但没有易用性功能。

统一构建工具的要求 📐：

- 能构建 ROS 1、ROS 2 工作空间，也能构建 Gazebo 及其依赖这种**非 ROS** 的包（没有 `package.xml` 时从源码推断，或者从外部提供元信息）。
- 提供"开发环境"：用户能方便地手动调用某个包的构建系统。
- 不止构建，还要测试。
- 名字和任何构建系统无关，避免误会。
- 用 Python 的入口点（entry point）机制做扩展，扩展可以放在单独的 Python 包里。原则：关注点分离、单一职责、最少知识、DRY、KISS、"不用的不付钱"。

列出的扩展点 📐：命令（verb）、命令行选项、包的发现、包的识别、包的处理（构建、测试）、执行模型（串行、并行）、输出处理、环境配置脚本（sh、bash、bat）、命令补全。

明确**不管**的事 📐：拉源码（vcstool）、装依赖（rosdep）、打二进制包（bloom）。

最后在 option A（改造 `catkin_tools`）和 option B（用 colcon）之间选了 **colcon**：它当时就能部署、覆盖 ROS 2 的用例，Bouncy 前要做的工作最少，架构更干净、Python 3.5+ 📐。结论是"不是全票通过，但绝大多数人支持或无所谓"📐。影响：**任何包的 CMake 代码都不用改**，命令从 `ament build` 变成 `colcon build`；colcon 不支持 devel space，要求包有安装规则 📐。

### 交叉编译的设计（草稿，2019-09）📐

思路是把当时教程里的步骤包成命令：

1. `create-cc-sysroot`：用 Docker + QEMU 拉一个目标平台的 ROS 2 基础镜像，`COPY` 工作空间进去、跑 `rosdep` 装依赖，再把容器的文件系统导出成 **sysroot**（目标机的根文件系统）。
2. 每个平台一个 **colcon mixin**，例如 `cc-generic_armhf-ubuntu_bionic-fastrtps-crystal`，给 CMake 传工具链文件（`CMAKE_TOOLCHAIN_FILE`）和 sysroot。
3. 第二阶段：维护一个带工具链和 mixin 的 Docker 镜像，再提供一个 `cc-build` 一步到位。

工作空间目录按"平台-系统-RMW-发行版"命名（cc-root）。用 QEMU 只在建基础镜像时（不可避免），编译用原生的交叉编译器，因为 QEMU 大约慢 10 倍 📐。备选方案是 `colcon bundle`：在目标平台容器里直接编译并打包，但用模拟的 ARM 编译器太慢 📐。局限：只针对 Linux 目标，Windows 不行 📐。

## 3. 设计与 Rolling 现状对照

状态：✅ 按设计实现 · 🔄 实现与设计不同 · ⏳ 未实现 · 🗑️ 已过时。🔍 结果来自[练习](../labs/security-build-design-check.md)第 4–7 部分。

| 设计文档的说法 | 状态 | Rolling 实测 |
|---|---|---|
| `ament_tools` 被 colcon 取代（Bouncy 起） | ✅ | 镜像里装的是 colcon-core 0.21.3 + `colcon-common-extensions` 的 20 多个扩展包，没有 `ament_tools` 🔍 |
| 不在同一个 CMake 上下文里构建多个包 | ✅ | 两个包都定义目标 `util`：colcon 正常构建两个包；用一个顶层 `CMakeLists.txt` `add_subdirectory` 两个包（模拟 `catkin_make`）报 `add_library cannot create target "util" because another target with the same name already exists` 🔍 |
| 用 `AMENT_PREFIX_PATH`，构建时推出 CMake 变量 | ✅ | `source /opt/ros/rolling/setup.bash` 后只有 `AMENT_PREFIX_PATH=/opt/ros/rolling`，**`CMAKE_PREFIX_PATH` 没设** 🔍。colcon-ros 在调用 CMake 前把 `AMENT_PREFIX_PATH` 加进 `CMAKE_PREFIX_PATH`（`colcon_ros/task/__init__.py`）🔍 |
| 不用构建工具也能手动构建 | ✅，但靠的是 PATH | 手动 `cmake` 一个 `find_package(rclcpp)` 的工程能成功，因为 CMake 会从 `PATH` 里的 `/opt/ros/rolling/bin` 推出前缀；把 `PATH` 改成 `/usr/bin:/bin` 就找不到 rclcpp 🔍 |
| 支持 `ament_cmake`、`cmake`、`ament_python` 构建类型；也能构建没有清单的包 | ✅ | `colcon list` 把没有 `package.xml` 的 CMake 工程识别为 `(cmake)`，只有 `setup.py` 的识别为 `(python)`，都能构建 🔍 |
| ament 资源索引决定"有哪些包" | ✅ | 标记文件 `share/ament_index/resource_index/packages/<pkg>` 是空文件（0 字节）；删掉它，`ros2 pkg prefix` 报 `Package not found`，库文件明明还在 🔍。无清单的 CMake 包只有 colcon 自己的索引（`share/colcon-core/packages/`），所以 `ros2 pkg` 也看不到它 🔍 |
| symlink install 通过覆盖 `install()` 实现 | ✅ | `ament_cmake_core/cmake/symlink_install/install.cmake` 里 `function(install signature)` 把 `DIRECTORY`、`FILES`、`PROGRAMS`、`TARGETS` 转给 `ament_cmake_symlink_install_*`，其他情况回落到 `_install()` 🔍。装出来的 `libutil.so`、`package.xml` 都是指向 build、src 的链接 🔍 |
| `ament_auto`（类似 catkin_simple） | ✅ | 包名 `ament_cmake_auto` 2.9.3，在 `ros-base` 里 🔍 |
| colcon 用入口点做扩展，列出的 9 类扩展点 | ✅ | `colcon extension-points` 列出 21 个扩展点，覆盖 verb、argument_parser、package_discovery、package_identification、task.build/test、executor、event_handler（输出）、shell（环境脚本）、argcomplete（补全）🔍 |
| 支持 `DESTDIR`（option B 的待办） | ✅ | `DESTDIR=/tmp/stage colcon build` 把文件装到 `/tmp/stage/<install 前缀>/...`，`install/` 下没有 🔍 |
| 类似 `catkin_tools profile` 的配置（option B 的待办） | 🔄 | 没有 profile；有 mixin（`colcon mixin`，默认仓库里列出 27 条：`release`、`asan-gcc`、`ccache`、`ninja`……）和 `colcon-defaults` 的默认参数文件 🔍 |
| 每个目标平台一个交叉编译 mixin | ⏳ | 默认 mixin 仓库里没有任何交叉编译的 mixin 🔍 |
| `cross_compile` 工具（`create-cc-sysroot`、`cc-build`） | 🗑️ | 本仓库文档：`cross_compile` 工具"不再支持"，替代方案是 `docker buildx` 构建多平台镜像 📄。工具仓库的 README 也写着已弃用，并说明它实际用的是 QEMU 模拟构建，不是设计里的"sysroot + 原生交叉编译器" 🔍 |
| 交叉编译需要目标平台的 sysroot | ✅ 道理仍成立 | 只给工具链文件、不给 sysroot：无依赖的 CMake 库能编成 AArch64（`readelf` 显示 `Machine: AArch64`）；依赖 rclcpp 的包，CMake **找到了宿主机 x86 的 rclcpp 配置**（`Found rclcpp: 33.2.0 (/opt/ros/rolling/...)`），然后在 `fastcdr` 上碰巧失败 🔍。给一个空 sysroot 并设 `CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY`，在第一个 `find_package(ament_cmake)` 就失败 🔍 |

## 4. 对写代码的影响（规划控制场景）

1. **每个包都要写完整的安装规则。** 没有 devel space，没装的东西下游找不到、`ros2 run` 也找不到 📐🔍。卡片 [ament_cmake](../cards/engineering/ament-cmake.md) 的坑 1 就是这个。
2. **别把多个包塞进一个 CMake 工程来"加速"。** 💡 目标名、全局变量、`ament_package()` 都会冲突；想加速就用 `--parallel-workers`、`ccache` mixin，或者只编改动的包（`--packages-up-to`、`--packages-select`）。
3. **调试"找不到包"时分清三套索引。** 💡 CMake 找包看 `CMAKE_PREFIX_PATH` 和 PATH 推出的前缀；`ros2 pkg`、pluginlib 看 ament 资源索引；colcon 看 `COLCON_PREFIX_PATH` 和自己的 `share/colcon-core/packages/`。三者对不上时，症状是"编得过但运行时找不到"或者反过来。
4. **交叉编译的关键是 sysroot，不是编译器。** 🔍 没有 sysroot 时 CMake 会悄悄用宿主机的 x86 配置，失败的位置很随机。💡 现在更常见的做法：
   - 简单情况：`docker buildx` 加 QEMU 在目标架构的容器里原生构建（慢，但最省事）📄。
   - 追求速度：从目标机或 `arm64v8/ros` 镜像导出 sysroot，配合工具链文件（本练习的 [aarch64-toolchain.cmake](../labs/design_lab/config/aarch64-toolchain.cmake)）和 `--cmake-args -DCMAKE_SYSROOT=...`。注意 rosidl 的代码生成器要在宿主机上运行（`CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER`）。
5. **团队共享的编译选项用 mixin 或 defaults 文件。** 💡 例如 `colcon build --mixin release ccache`，或者在工作空间里放 `colcon.meta`、`~/.colcon/defaults.yaml`，比在 README 里写一长串参数可靠。

## 5. 与其他概念的关系

- 卡片：[工作空间与 colcon](../cards/engineering/workspace-colcon.md) · [ament_cmake](../cards/engineering/ament-cmake.md) · [rosdep](../cards/engineering/rosdep.md) · [pluginlib](../cards/engineering/pluginlib.md)（ament 资源索引）· [Docker 部署](../cards/middleware/docker-deploy.md)
- 练习：[编译与依赖的常见错误](../labs/build-pitfalls.md) · [安全与工程化的设计还成立吗](../labs/security-build-design-check.md)（第 4–7 部分）
- [为什么 ROS 2 选 DDS](why-ros2-dds.md)：同样是"不破坏 ROS 1"的约束
- [接口的设计](interface-design.md)：rosidl 代码生成在构建时发生
- 上一级：[总览脑图](../map/overview.md)

## 6. 自测题（先自己答，再看[答案](../answers/build-system-design-answers.md)）

1. 同事抱怨"colcon 太慢，我把 `nav_core`、`nav_planner`、`nav_controller` 三个包改成一个顶层 `CMakeLists.txt` 里 `add_subdirectory` 三个目录，一次 cmake 搞定"。（a）按设计文档，这种做法会遇到哪些问题？至少说两个。（b）这三个包现在都有一个叫 `${PROJECT_NAME}_lib` 的目标，会冲突吗？如果都叫 `core` 呢？（c）不合并包，有哪些办法让日常编译变快？
2. 你要把导航包部署到 Jetson（aarch64），在 x86 笔记本上执行 `colcon build --cmake-args -DCMAKE_TOOLCHAIN_FILE=aarch64.cmake`，工具链文件只设了编译器。（a）根据本篇的实测，CMake 会去哪里找 `rclcpp`？为什么这很危险？（b）要正确地交叉编译，还缺什么？说出至少两件事。（c）如果你不想维护 sysroot，有什么替代方案？代价是什么？
