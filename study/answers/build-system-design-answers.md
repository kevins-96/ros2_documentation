# 答案：构建系统的设计

> 对应：[build-system-design.md](../design/build-system-design.md)

## 第 1 题

**（a）** 这就是 `catkin_make` 的"单一 CMake 上下文"，ament 故意不支持它 📐。会遇到的问题：

1. **所有名字共用一个命名空间** 📐：目标、函数、宏、测试、全局变量都必须在三个包之间唯一。一个包里 `set(...)` 的变量会泄露到后面的包里，出现"单独编没问题、合起来就坏了"的副作用。
2. **包之间的 `find_package` 失效** 💡：`nav_planner` 里的 `find_package(nav_core)` 要找的是**安装好的** `nav_coreConfig.cmake`，而单上下文里 `nav_core` 还没安装。要改成直接用目标名，并手动声明跨包的目标依赖 📐，这就要求了解别的包的内部结构，违背了"包之间解耦"。
3. **工具链断了** 💡：`colcon build --packages-select`、`colcon test-result`、rosdep、bloom 打包都是按包工作的，合并后这些都用不了；发布二进制包时还得拆回去。

**（b）**

- `${PROJECT_NAME}_lib` 展开后是 `nav_core_lib`、`nav_planner_lib`、`nav_controller_lib`，名字不同，**不冲突** 💡。
- 都叫 `core` 会冲突：实测两个包都定义目标 `util`，合进一个上下文时报 `add_library cannot create target "util" because another target with the same name already exists` 🔍。分开用 colcon 构建则没有问题 🔍。

**（c）不合并包也能变快** 💡：

- 只编需要的：`colcon build --packages-up-to nav_controller` 或 `--packages-select nav_planner`。
- 不删 `build/` 目录，让 CMake 增量编译；Python 包和配置文件用 `--symlink-install`，改了不用重新安装 📐。
- 用 mixin：`--mixin ccache ninja`；日常迭代时 `--mixin build-testing-off` 跳过测试目标（这几个 mixin 在默认仓库里都有 🔍）。
- 调 `--parallel-workers` 和 `MAKEFLAGS`，在包级别和编译单元级别都并行。

## 第 2 题

**（a）在宿主机的 `/opt/ros/rolling` 里找** 🔍。colcon-ros 会把 `AMENT_PREFIX_PATH` 加进 `CMAKE_PREFIX_PATH` 🔍，而工具链文件没有限制查找范围。实测 CMake 打印 `Found rclcpp: 33.2.0 (/opt/ros/rolling/share/rclcpp/cmake)`，找到的是 x86 那份 🔍。

危险在于：`share/` 下的 CMake 配置和架构无关，所以**配置阶段可能一路通过**，直到某个依赖恰好在多架构目录里（实测是 `lib/x86_64-linux-gnu/cmake/fastcdr`）才失败 🔍。换一套依赖，也可能一直走到链接阶段，才报"文件格式不对"这类错误 💡。失败的位置很随机，报错也不会直接告诉你"缺 sysroot"。

**（b）还缺：**

1. **目标平台的 sysroot**：里面要有 aarch64 版的 ROS 2（同一个发行版、相近的同步时间）和所有系统依赖（Eigen、Boost、OpenCV……）📐。设计文档的做法是用 Docker + QEMU 跑目标平台的镜像，`rosdep install` 之后导出文件系统。
2. **工具链文件里设好查找规则**：`CMAKE_SYSROOT`，`CMAKE_FIND_ROOT_PATH_MODE_PACKAGE/LIBRARY/INCLUDE` 设成 `ONLY`，`CMAKE_FIND_ROOT_PATH_MODE_PROGRAM` 设成 `NEVER` 🔍（本练习的 [aarch64-toolchain.cmake](../labs/design_lab/config/aarch64-toolchain.cmake)）。设了 `ONLY` 之后，没有 sysroot 就会在第一个 `find_package(ament_cmake)` 失败，至少错在明处 🔍。
3. **宿主机上能运行的代码生成器** 💡：rosidl 的生成器是 Python 脚本，要在宿主机上运行，所以宿主机也要装同一版本的 ROS 2，而 `PROGRAM` 的查找不能限制在 sysroot 里。
4. **部署时目标机上的运行时依赖** 📐：设计文档假设把整个工作空间拷到设备上，再在设备上 `rosdep install`。

**（c）替代方案：在目标架构的容器里原生构建** 📄。例如 `docker buildx build --platform linux/arm64`，或者 `docker run --platform linux/arm64 ros:rolling-ros-base` 后在里面 `colcon build`，靠 QEMU 的 `binfmt_misc` 运行 ARM 程序。

- 好处：不用维护 sysroot 和工具链文件，环境和目标机一致 💡。
- 代价：QEMU 模拟大约慢 10 倍 📐，大工作空间要编很久。这也是设计文档只在"建基础镜像"时用 QEMU、编译时用原生交叉编译器的原因 📐。
- 另一个办法是直接在 Jetson 上编，或者用 ARM 的 CI 机器 💡。
