# 路线图：卡片与源文档对照

> 目的：说清楚每张卡片"吸收了哪些源文档"、哪些源文档有意不收。想核对原文时从这里跳。
> 第 1–7 批的路径都相对仓库的 `source/` 目录；第 8 批起的设计文档在 [ros2/design](https://github.com/ros2/design) 的 `articles/` 里。✅ = 已完成。

## 编写约定（所有批次通用）

- 发行版 Rolling；C++ 代码在 `ros:rolling-ros-base` 镜像里编译实测，结果写进卡片或练习。
- 卡片固定六段：一句话定义 → 为什么需要 → 最小 C++ 代码 → 常见坑 → 与其他概念的关系 → 2 道自测题（答案在 `answers/`）。
- 来源标记：📄 文档原文 · 🔍 源码或实测核实 · 💡 经验补充 · 📐 [ros2/design](https://github.com/ros2/design) 设计文档原文（第 8 批起）。
- 设计笔记（`design/`）固定六段：设计要回答的问题 → 考虑过的方案与取舍 → 设计与 Rolling 现状对照 → 对写代码的影响 → 与其他概念的关系 → 2 道自测题。
- **全库文件名不重名**（卡片 `xxx.md`，答案 `xxx-answers.md`），以后改用 `[[wikilink]]` 只需按文件名替换。每张卡片的 frontmatter 有 `aliases`，Obsidian 里搜中文名也能找到。

## 第 1 批：让模块互相说话（通信）✅

| 卡片 | 吸收的源文档 |
|---|---|
| ✅ [节点与 ROS 图](cards/communication/node.md) | `ROS-Framework/About-Nodes.rst`、`How-ROS-Works.rst`、`nodes/Working-with-nodes/Understanding-ROS2-Nodes/*`、`nodes/Working-with-nodes/Using-Sub-Nodes.rst`、`_internal/Rclcpp-Convenience-Header-Note.rst` |
| ✅ [通信方式怎么选](cards/communication/communication-patterns.md) | `ROS-Framework/Interfaces-Topics-Services-Actions.rst` |
| ✅ [话题](cards/communication/topic.md) | `interfaces/About-Topics.rst`、`interfaces/topics/Understanding-ROS2-Topics/*`、`client-libraries/Working-with-Client-Libraries/Writing-A-Simple-Cpp-Publisher-And-Subscriber.rst` |
| ✅ [QoS](cards/communication/qos.md) | `interfaces/topics/About-Quality-of-Service-Settings.rst`、`Working-with-topics/Quality-of-Service.rst`、`Working-with-topics/Configuring-QoS-Overrides.rst` |
| ✅ [服务](cards/communication/service.md) | `interfaces/About-Services.rst`、`services/Working-with-services/Sync-Vs-Async.rst`、`Understanding-ROS2-Services/*`、`Writing-A-Simple-Cpp-Service-And-Client.rst` |
| ✅ [动作](cards/communication/action.md) | `interfaces/About-Actions.rst`、`actions/Working-with-actions/Understanding-ROS2-Actions/*`、`Creating-an-Action.rst`、`Writing-an-Action-Server-Client/Cpp.rst` |
| ✅ [接口定义](cards/communication/interface-definition.md) | `Working-with-interfaces/Interface-specifications.rst`、`Custom-ROS2-Interfaces.rst`、`Single-Package-Define-And-Use-Interface.rst` |
| ✅ [话题进阶](cards/communication/topic-advanced.md) | `Working-with-topics/Content-Filtering-Subscription.rst`、`Topic-Keys/Topic-Keys-Tutorial.rst`、`Topic-Keys/Filtered-Topic-Keys-Tutorial.rst`、`Working-with-topics/Wait-for-Acknowledgment.rst` |

练习：✅ [QoS 不匹配](labs/qos-mismatch.md)、✅ [动作抢占](labs/action-preempt.md)。

Python 版教程（`*-Py.rst`、`*-Python.rst`）只用于核对概念，卡片代码以 C++ 为准。

## 第 2 批：知道东西在哪（坐标与机器人模型）✅

| 卡片 | 吸收的源文档 |
|---|---|
| ✅ [tf2 坐标变换](cards/coordinates/tf2.md) | `interfaces/Working-with-interfaces/About-Tf2/*`、`Introduction-To-Tf2/*`、`client-libraries/Working-with-Client-Libraries/Tf2/Tf2-Main.rst`、`Writing-A-Tf2-Static-Broadcaster-Cpp.rst`、`Writing-A-Tf2-Broadcaster-Cpp.rst`、`Writing-A-Tf2-Listener-Cpp.rst`、`Adding-A-Frame-Cpp.rst`、`Developer-Tools/Debugging/Debugging-Tf2-Problems/*`（工具部分） |
| ✅ [tf2 与时间](cards/coordinates/tf2-time.md) | `Tf2/Learning-About-Tf2-And-Time-Cpp.rst`、`Time-Travel-With-Tf2-Cpp.rst`、`Using-Stamped-Datatypes-With-Tf2-Ros-MessageFilter.rst`、`Debugging-Tf2-Problems/*`（时间部分） |
| ✅ [四元数](cards/coordinates/quaternion.md) | `Working-with-interfaces/Quaternion-Fundamentals.rst` |
| ✅ [URDF 与机器人模型](cards/coordinates/urdf.md) | `Capabilities/Simulation/URDF/*`（6 篇）、`Developer-Tools/Build/Using-Xacro-to-Clean-Up-a-URDF-File/*`、`Capabilities/Simulation/dummy-robot/*` |

练习：✅ [lookupTransform 的时间参数](labs/tf-time-lookup.md)、✅ [四元数与 tf2 缓冲](labs/quaternion-buffer.md)、✅ [URDF 关节与 TF](labs/urdf-joint-states.md)。

本批实测发现的文档与 Rolling 不一致之处（详见各卡片"常见坑"）：广播器构造传 `this` 编译不过；MessageFilter 教程 3 处 API 过时；`tf2_echo` 参数名与实际语义相反；xacro 找不到宏时现在会报错。

Python 版（`Tf2/*-Py.rst`、`Using-URDF-with-Robot-State-Publisher-py.rst`）只用于核对概念。

`Buffer-Backends/*`（`rosidl::Buffer`、GPU 内存传输）原计划放在本批，读后发现它讲的是消息内存而不是 tf2 缓冲，移到第 4 批"进程内通信与零拷贝"。

## 第 3 批：让节点可配置可启停（配置与启停）✅

| 卡片 | 吸收的源文档 |
|---|---|
| ✅ [参数](cards/configuration/parameter.md) | `ROS-Framework/About-Parameters.rst`、`parameters/Working-with-parameters.rst`、`Understanding-ROS2-Parameters/*`、`Using-ros2-param.rst`、`client-libraries/.../Using-Parameters-In-A-Class-CPP.rst`、`Monitoring-For-Parameter-Changes-CPP.rst`、`Launch/Node-arguments.rst`（参数部分） |
| ✅ [生命周期节点](cards/configuration/lifecycle-node.md) | `nodes/Working-with-nodes/Managed-Nodes.rst`、`About-Nodes.rst`（Lifecycle 段）、`nodes/Working-with-nodes/Using-Node-Interfaces-Template-Class.rst` |
| ✅ [launch 启动系统](cards/configuration/launch.md) | `Developer-Tools/About-Launch.rst`、`Launch/Launch-Main.rst`、`Launch-system.rst`、`Creating-Launch-Files.rst`、`Launch-file-different-formats.rst`、`Using-Substitutions.rst`、`Using-Event-Handlers.rst`、`Using-ROS2-Launch-For-Large-Projects.rst`、`nodes/.../Launching-Multiple-Nodes/*` |
| ✅ [启动参数与重映射](cards/configuration/node-arguments.md) | `Launch/Node-arguments.rst`、`Get-Started/Configuring-ROS2-Environment.rst`（source 部分） |
| ✅ [日志](cards/configuration/logging.md) | `nodes/About-Logging/*`、`Learning-About-Logging/*`、`Using-Rqt-Console/*`、`Developer-Tools/Introspection-and-analysis/Logging-and-logger-configuration.rst`、`Launch/Node-arguments.rst`（日志部分） |

练习：✅ [参数的规则与陷阱](labs/parameter-rules.md)、✅ [生命周期状态切换](labs/lifecycle-transitions.md)、✅ [launch 与名字解析](labs/launch-and-names.md)。代码在 `labs/config_lab/`。

调整说明：
- 原计划的"节点接口模板"并入[生命周期节点](cards/configuration/lifecycle-node.md)：`NodeInterfaces<>` 的主要用途正是让工具函数同时接受 `rclcpp::Node` 和 `LifecycleNode`。
- `Configuring-ROS2-Environment.rst` 里 Domain ID、`ROS_AUTOMATIC_DISCOVERY_RANGE` 等发现相关内容放到第 7 批"Domain ID 与发现"。
- `Managed-Nodes.rst` 只有概述和外链，生命周期卡片的状态机细节来自 `rclcpp_lifecycle` 源码和实测。

本批实测发现的值得注意之处（详见各卡片"常见坑"）：启动时给值不经过参数回调；double 参数启动时传整数会让节点崩溃；`LaunchConfiguration` 在 Python 里不能直接比较；命名空间下 `--log-level` 要写 `命名空间.节点名`；仓库示例 `different_formats_launch.py` 的 `PushROSNamespace` 用了字面量而不是 launch 参数。

## 第 4 批：让回调按时跑（并发与性能补全）✅

| 卡片 | 吸收的源文档 |
|---|---|
| ✅ [执行器](cards/concurrency/executor.md)、✅ [回调组](cards/concurrency/callback-group.md)（样板） | `client-libraries/About-Executors/*`、`nodes/Working-with-nodes/Using-callback-groups.rst` |
| ✅ [组件与组合](cards/concurrency/component.md) | `nodes/About-Composition.rst`、`nodes/Working-with-nodes/Composition.rst`、`Writing-a-Composable-Node.rst`、`Developer-Tools/Launch/Launching-composable-nodes.rst` |
| ✅ [进程内通信与零拷贝](cards/concurrency/intra-process.md) | `nodes/Working-with-nodes/intra-process/*`、`client-libraries/Working-with-Client-Libraries/Configure-ZeroCopy-loaned-messages.rst`、`interfaces/Working-with-interfaces/Buffer-Backends/About-Buffer-Backends.rst`、`Using-Buffer-Backends.rst` |
| ✅ [实时编程](cards/concurrency/realtime.md) | `Capabilities/Motion-planning/Real-Time-Programming.rst`、`client-libraries/Working-with-Client-Libraries/Allocator-Template-Tutorial.rst`、`Developer-Tools/Build/Building-Realtime-rt_preempt-kernel-for-ROS-2/*`（只收结论） |

练习：✅ [组件与容器](labs/component-container.md)、✅ [进程内通信与零拷贝](labs/intra-process-zero-copy.md)、✅ [实时循环的抖动](labs/realtime-jitter.md)。代码在 `labs/perf_lab/`。

调整说明：
- `Buffer-Backends` 里写后端插件（`Writing-a-Buffer-Backend.rst`、`Writing-a-Buffer-Compatible-Conversions-Package.rst`）和 GPU 演示（`GPU-Buffer-Transport.rst`）面向厂商和 CUDA 环境，没有收；卡片只收用户需要知道的部分：`uint8[]` 在 Rolling 生成 `rosidl::Buffer`、订阅端怎么选后端。
- RT_PREEMPT 内核编译教程基于 Ubuntu 20.04 / 5.4 内核，步骤已过时，只收"为什么需要 RT 内核"的结论。
- 原计划的"客户端库内部结构（可选）"（`About-Client-Libraries.rst`、`About-Internal-Interfaces/*`）暂不做，需要时再补。

本批实测发现的值得注意之处（详见各卡片"常见坑"）：Rolling 里 `uint8[]` 字段变成 `rosidl::Buffer`，部分旧代码写法编译失败；进程内通信 + `KeepAll` 会抛异常；单线程容器里组件构造函数阻塞会让整个容器停住；容器的命名空间不传给组件；跨进程传 1 MB 以上点云在 SensorDataQoS 下几乎全丢；`rclcpp_components` 头文件要求 C++20。

## 第 5 批：把代码变成能交付的包（工程化）✅

| 卡片 | 吸收的源文档 |
|---|---|
| ✅ [工作空间与 colcon](cards/engineering/workspace-colcon.md) | `client-libraries/Working-with-Client-Libraries/Creating-A-Workspace/*`、`Colcon-Tutorial.rst`、`Creating-Your-First-ROS2-Package.rst`、`Developer-Tools/Build/Using-Variants.rst`、`Build/About-Build-System.rst`（构建工具部分） |
| ✅ [ament_cmake](cards/engineering/ament-cmake.md) | `Developer-Tools/About-Build.rst`、`Build/About-Build-System.rst`、`Build/Ament-CMake-Documentation.rst`、`Build/Developing-a-ROS-2-Package.rst`、`Releases/Release-Kilted-Kaiju.rst`（`ament_target_dependencies` 弃用）、`Releases/Release-Humble-Hawksbill.rst`（头文件目录） |
| ✅ [rosdep 依赖管理](cards/engineering/rosdep.md) | `client-libraries/Working-with-Client-Libraries/Rosdep.rst`、`Developer-Tools/Build/Supplementing-Custom-Rosdep-Keys.rst` |
| ✅ [pluginlib 插件](cards/engineering/pluginlib.md) | `client-libraries/Working-with-Client-Libraries/Pluginlib.rst`、`Ament-CMake-Documentation.rst`（ament 索引部分） |
| ✅ [测试](cards/engineering/testing.md) | `Developer-Tools/About-testing.rst`（目录页）、`Debugging/About-testing.rst`、`Debugging/Testing/Testing.rst`、`Testing/Cpp.rst`、`Testing/Integration.rst`、`Testing/CLI.rst`、`Build/Ament-Lint-For-Clean-Code.rst` |

练习：✅ [编译与依赖的常见错误](labs/build-pitfalls.md)、✅ [用插件换路径平滑器](labs/plugin-smoother.md)、✅ [测试与代码规范检查](labs/testing-and-lint.md)。代码在 `labs/build_lab/`（`path_smoother_core`、`path_smoother_plugins` 两个包）。

调整说明：
- `Ament-Lint-For-Clean-Code.rst` 原计划放在 ament_cmake，实际并入"测试"卡片，因为 linter 是通过 `colcon test` 运行的。
- `Testing/Testing.rst` 讲的是如何安装预发布（ros-testing）二进制来帮忙测试 ROS 本身，和测试自己的代码关系不大，只在卡片源文档里列出，不展开。
- `Using-Variants.rst` 内容很短（元包的概念和私有变体写法），并入"工作空间与 colcon"的概念表。

本批实测发现的值得注意之处（详见各卡片"常见坑"）：Rolling 已删除 `ament_target_dependencies`；`colcon test` 有失败时退出码仍为 0，`colcon test-result` 会读到旧的 XML；在 source 了某个工作空间的终端里编译，会把它永久写进新工作空间的 `setup.bash`；pluginlib 6.0.3 写了 `name` 后不能再用 `type` 查找（与文档说法不符），`ros2 plugin list` 正常不代表能加载；`add_library` 不写 `SHARED` 时库类型取决于是否 `find_package(rclcpp)`；镜像里 cppcheck 2.19 被静默跳过；离线时 xmllint 校验 `package.xml` 失败；`rosdep --ignore-src` 会把 underlay 中已有的包视为"有源码"。

## 第 6 批：看见系统在干什么（调试与观测）✅

| 卡片 | 吸收的源文档 |
|---|---|
| ✅ [命令行工具](cards/observability/cli-tools.md) | `Developer-Tools/Introspection-and-analysis/About-Command-Line-Tools.rst`、`client-libraries/Working-with-Client-Libraries/Getting-Started-With-Ros2doctor.rst`、`Introspection-and-analysis/Publishing-Messages-Using-YAML-Files.rst` |
| ✅ [rosbag 录制回放](cards/observability/rosbag.md) | `interfaces/Working-with-interfaces/Recording-And-Playing-Back-Data/*`、`client-libraries/Working-with-Client-Libraries/Recording-A-Bag-From-Your-Own-Node-CPP.rst`、`Reading-From-A-Bag-File-CPP.rst`、`Developer-Tools/Debugging/Testing/Overriding-QoS-Policies-For-Recording-And-Playback.rst` |
| ✅ [服务与动作内省](cards/observability/service-introspection.md) | `Introspection-and-analysis/Service-Introspection.rst`、`interfaces/actions/Working-with-actions/Action-Introspection.rst` |
| ✅ [调试与回溯](cards/observability/gdb-debugging.md) | `Developer-Tools/About-Debugging.rst`、`Debugging/Getting-Backtraces-in-ROS-2.rst`、`Debugging/ROS-2-IDEs.rst` |
| ✅ [tracing 性能追踪](cards/observability/tracing.md) | `Developer-Tools/About-introspection-and-analysis.rst`、`Introspection-and-analysis/Tracing/ROS2-Tracing-Trace-and-Analyze.rst`、`Debugging/Building-ROS-2-with-Tracing.rst` |
| ✅ [RViz 与可视化](cards/observability/rviz.md) | `Developer-Tools/About-visualization.rst`、`Visualization/RViz/RViz-Main.rst`、`RViz-User-Guide/*`、`Marker-Display-types/*`、`Marker-Sending-Basic-Shapes/*`、`Marker-Points-and-Lines/*`、`Visualization/About-RQt.rst` |

练习：✅ [命令行与服务内省](labs/cli-and-introspection.md)、✅ [录制、回放与离线分析](labs/bag-record-replay.md)、✅ [崩溃回溯与回调追踪](labs/crash-and-trace.md)。代码在 `labs/observe_lab/`。

调整说明：
- 原计划放在"命令行工具"的 `Examine-Traffic.rst` 读后发现讲的是用 tcpdump 对比开启 SROS2 前后的网络流量（看数据是否加密），移到第 7 批"安全 SROS2"。
- 服务内省原计划只是"命令行工具"的一部分，内容较多（三种状态、四种事件、录制服务和动作），单独成卡。
- `RViz-Custom-Display/*`、`RViz-Custom-Panel/*`（写 RViz 插件）没有收，需要时再补；`Rqtbag/Create-An-Rqtbag-Plugin.rst` 维持"有意不收"。
- tracing 部分：容器里 ROS apt 源不可用，没能安装 `ros2trace` 和 `tracetools_analysis`，改用 `lttng` 命令 + `babeltrace2` + 自写的 [callback_durations.py](labs/observe_lab/scripts/callback_durations.py) 实测；`ros2 trace` 的用法按文档标 📄。
- RViz 部分：镜像没有图形界面，RViz 操作按文档标 📄，Marker 字段默认值和 tf2 对 frame 名的检查在容器里实测。
- `Debugging-Tf2-Problems/*` 已在第 2 批收进 tf2 卡片；`Logging-and-logger-configuration.rst` 已在第 3 批收进日志卡片。

本批实测发现的值得注意之处（详见各卡片"常见坑"）：Rolling 的 `ros2 bag record` 必须写 `--topics`，位置参数会报错；`create_subscription<rclcpp::SerializedMessage>` 编译通过但运行时抛异常；进程崩溃时 bag 没有 `metadata.yaml`；文档 YAML 发布示例的输出顺序（1、3、2）是笔误；Rolling 的 rmw_fastrtps 会多出 `<话题>/_buf_cpu` 隐藏话题；gdb 批处理模式会把崩溃的退出码变成 0；不设 `CMAKE_BUILD_TYPE` 没有行号，Release 下出错的小函数会被内联消失；定时器回调超过周期时不补发、频率悄悄下降；`Marker` 默认 alpha 为 0；tf2 拒绝以 `/` 开头的 frame 名，而文档的 Marker 示例写的是 `"/my_frame"`。

## 第 7 批：让多台机器连起来（中间件与部署）✅

| 卡片 | 吸收的源文档 |
|---|---|
| ✅ [中间件 RMW 与 DDS](cards/middleware/rmw-dds.md) | `ROS-Framework/About-Middleware.rst`、`middleware/About-Middleware-Implementations.rst`、`About-Different-Middleware-Vendors.rst`、`Get-Started/Installation/RMW-Implementations/*`（含 Zenoh）、`Get-Started/Installation/RMW-Implementations/Working-with-multiple-RMW-implementations.rst` |
| ✅ [Domain ID 与发现](cards/middleware/domain-discovery.md) | `nodes/About-Domain-ID.rst`、`Get-Started/Configuring-ROS2-Environment.rst`（Domain ID 与发现部分，从第 3 批移来）、`Launch/Improved-Dynamic-Discovery.rst`、`Launch/Discovery-Server/*` |
| ✅ [DDS 调优与 Fast DDS XML](cards/middleware/dds-tuning.md) | `Developer-Tools/Build/DDS-tuning.rst`、`Launch/FastDDS-Configuration.rst` |
| ✅ [Docker 部署](cards/middleware/docker-deploy.md) | `Build/Run-2-nodes-in-single-or-separate-docker-containers.rst`、`Setup-ROS-2-with-VSCode-and-Docker-Container.rst`、`Deployment-Guidelines/*`、`Build/About-Cross-Compilation.rst`、`Cross-compilation.rst` |
| ✅ [安全 SROS2](cards/middleware/sros2-security.md) | `Launch/Security/*`、`Build/The-Keystore.rst`、`Introspection-and-analysis/Examine-Traffic.rst`（从第 6 批移来） |

练习：✅ [中间件、Domain 与发现](labs/middleware-discovery.md)、✅ [DDS 调优、XML 配置与容器网络](labs/dds-and-docker.md)、✅ [SROS2 密钥库、强制模式与访问控制](labs/sros2-keystore.md)。代码和配置在 `labs/mw_lab/`。

调整说明：
- 计划中的"DDS 调优"扩成"DDS 调优与 Fast DDS XML"：Fast DDS 的 XML profile（发布模式、分区、匹配数上限）和内核参数放在一起讲。Discovery Server 和 Super Client 的 XML 放在"Domain ID 与发现"。
- 镜像里只有 `rmw_fastrtps_cpp`，ROS apt 源在容器里不可用，Cyclone DDS、Connext、Zenoh 没有实测，按文档标 📄；Zenoh 路由器的可执行文件名、环境变量名对照 `rmw_zenoh` 源码核实。
- 交叉编译两篇文档内容很少（`Cross-compilation.rst` 正文为空），并入 Docker 部署卡片的一小节。
- 多台机器用两个 Docker 容器（同一个 bridge 网络）模拟；SROS2 的抓包在 `lo` 上用 `tcpdump` 做。

本批实测发现的值得注意之处（详见各卡片"常见坑"）：两个 `network_mode: host` 的容器默认互相收不到（Fast DDS 走共享内存而 `/dev/shm` 不共享），没有任何报错；`ros2 daemon` 只按 Domain ID 和 RMW 区分，后来改的 `ROS_DISCOVERY_SERVER` 等变量它看不到；文档的 Super Client XML 在 Fast DDS 3 里解析失败并被整个忽略，`fastdds discovery -i` 已弃用；Fast DDS XML 教程对默认发布模式的说法自相矛盾，源码是同步；XML profile 名必须带 `/`，分区和匹配数上限在 CLI 里看不出来，匹配数上限时被拒的订阅端仍报告 `matched=1`；`nav_msgs/Path` 序列化比同样字节数的 `PointCloud2` 慢约 50 倍；`ipfrag_high_thresh` 新内核默认 4 MB 而不是文档说的 256 KB；SROS2 不设 `Enforce` 时配错会悄悄降级，permissions 绑定创建时的 Domain ID，改名或重映射会被访问控制在启动时拒绝，加密后 enclave 名字仍是明文；容器里 root 生成的私钥权限是 `666`；部署教程的 Dockerfile 下载链接 404、默认 `humble`，预期输出里 talker/listener 写反；安全教程的 `zenohd` 应为 `rmw_zenohd`。

## 第 8 批：设计理念 · 通信与中间件 ✅（待合并）

来源换成了 [ros2/design](https://github.com/ros2/design)（Apache-2.0，固定在提交 `93a415b`，2026-11-23 读取）。设计文档讲"为什么这样设计"，很多是早期提案，所以每篇笔记都有一张"设计 vs Rolling 现状"对照表，标 📐 的句子只代表设计意图。设计文档原文没有复制进 `study/`，笔记里的引用都链接到固定提交，原文以后改了也不影响对照。想离线读原文，可以用仓库根目录的 git submodule `external/ros2_design/`，它固定在同一个提交，用法见 [README](README.md)。

| 设计笔记 | 吸收的设计文档（`articles/`） |
|---|---|
| ✅ [为什么有 ROS 2、为什么建在 DDS 上](design/why-ros2-dds.md) | [`010_why_ros2.md`](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/010_why_ros2.md)、[`020_ros_with_dds.md`](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/020_ros_with_dds.md)、[`030_ros_with_zeromq.md`](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/030_ros_with_zeromq.md)、[`040_stories.md`](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/040_stories.md)、[`changes.md`](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/changes.md) |
| ✅ [ROS 图怎么映射到 DDS](design/graph-to-dds-mapping.md) | [`140_topic_and_service_name_mapping.md`](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/140_topic_and_service_name_mapping.md)、[`170_node_to_participant_mapping.md`](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/170_node_to_participant_mapping.md) |
| ✅ [QoS 的设计](design/qos-design.md) | [`qos.md`](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/qos.md)、[`qos_deadline_liveliness_lifespan.md`](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/qos_deadline_liveliness_lifespan.md)、[`qos_configurability.md`](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/qos_configurability.md) |
| ✅ [服务与动作的设计](design/service-action-design.md) | [`050_ros_rpc_design.md`](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/050_ros_rpc_design.md)、[`actions.md`](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/actions.md)、[`content_filtering.md`](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/content_filtering.md)（动机部分） |
| ✅ [接口定义的设计](design/interface-design.md) | [`110_interface_definition.md`](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/110_interface_definition.md)（已被取代，只核对）、[`111_mapping_dds_types.md`](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/111_mapping_dds_types.md)、[`112_generated_interfaces_cpp.md`](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/112_generated_interfaces_cpp.md)、[`115_idl.md`](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/115_idl.md)、[`116_legacy_interface_definition.md`](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/116_legacy_interface_definition.md)、[`117_wide_strings.md`](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/117_wide_strings.md) |

练习：✅ [设计文档的说法在 Rolling 里还成立吗](labs/design-claims-check.md)，代码在 `labs/design_lab/`（5 个程序 + 抓包脚本），`study_interfaces` 新增 `FixedGrid.msg`。现有卡片（节点、话题、QoS、服务、动作、接口定义、话题进阶、通信方式、RMW 与 DDS、SROS2、组件）顶部加了"📐 设计动机"链接。

调整说明：
- 设计文档不按原目录收，按"学完哪批卡片后读"分组；每篇笔记对应 2–6 篇设计文档。
- `114_generated_interfaces_python.md` 不收（以 C++ 为主）；`060_ros_middleware_interface.md` 和 `serialization.md` 放到第 10 批，和零拷贝、进程内通信一起讲。
- 实测环境：`ros:rolling-ros-base`，rclcpp 33.2.0，rcl / rcl_action 10.6.0，`rmw_fastrtps_cpp` 9.6.1，Fast DDS 3.6.2，`rosidl_generator_cpp` 5.3.2。只测了 Fast DDS。

本批实测发现的设计与现状差异（详见各笔记的对照表）：
- **名字**：设计禁止 `__`，实际接受；`rostopic://` 写法没实现；长度上限是完整名字 247 个字符（`255U - 8U`），不是"含前缀 256"；`avoid_ros_namespace_conventions` 只去掉 `rt` 前缀，命名空间和开头的 `/` 照样保留；设计里的 `rs`/`rp`/`ra` 前缀没有用上，参数就是普通服务，动作走 `_action/` 名字。
- **参与者**：一个 Context 一个参与者（Foxy 起），`ros_discovery_info` 在 rclcpp 里订阅不到；`ignore_local_publications` 忽略的是同一个参与者，同进程的兄弟节点、组件也被忽略。
- **QoS**：Deadline 只在 rmw 层计量；事件会合并（一次回调报 `total_count_change=10`）；`MANUAL_BY_NODE` 已删，新增 `BestAvailable`；Fast DDS 上同一个参与者里的 AUTOMATIC 发布后约 300 ms 误报 not_alive；服务和动作不支持 Deadline 等；外部配置的"隐藏参数"没实现。
- **服务与动作**：协议里没有超时，超时的请求要自己清理；服务不能取消；rclcpp_action 客户端**用了** `status` 话题（设计说只用于内省）；结果默认缓存 10 s，过期后 `status` 话题不会重发，迟到的订阅者仍看到 SUCCEEDED；反馈内容过滤默认关闭，最多 6 个目标（参数上限 100，每个 UUID 占 16）。
- **接口**：`byte` → `unsigned char`、`char` → `uint8_t`（设计是 `std::byte`、`char`）；有界字符串在赋值、序列化、反序列化时都不检查；常量生成 `static constexpr` 而不是 `enum`；`MessageInitialization::ZERO` 反而最慢（生成代码把数组清零两次），`DEFAULTS_ONLY` 接近 0；Fast DDS 类型支持直接序列化 ROS 结构体，没有设计里那次"拷贝成 DDS 类型"。
- **整体**：`rmw_zenoh_cpp` 不是 DDS，Kilted 起是 Tier 1（设计里"所有 rmw 都基于 DDS"已过时）。

## 第 9–11 批：设计理念（计划，按顺序做，每批一个 PR）

| 批次 | 设计笔记（暂定） | 设计文档（`articles/`） | 学完哪批卡片后读 |
|---|---|---|---|
| 第 9 批：配置与启停的设计 | 参数的设计；名字、重映射与命令行参数的设计；launch 的设计；生命周期节点的设计；时钟与时间的设计 | `055_ros_parameter_design.md`、`141_static_remapping.md`、`160_ros_command_line_arguments.md`、`150_roslaunch.md`、`151_roslaunch_xml.md`、`152_roslaunch_frontend.md`、`node_lifecycle.md`、`130_ros_time.md` | 第 2、3 批 |
| 第 10 批：并发与性能的设计 | 实时系统的设计；进程内通信与零拷贝的设计；rmw 接口与序列化的设计 | `120_realtime_background.md`、`121_realtime_proposal.md`、`intraprocess_communication.md`、`zero_copy.md`、`060_ros_middleware_interface.md`、`serialization.md`、`unique_network_flows.md` | 第 4、7 批 |
| 第 11 批：安全与工程化的设计 | DDS 安全与 enclave 的设计；威胁模型；构建系统的设计；发现与协商 | `180_ros2_dds_security.md`、`181_ros2_access_control_policies.md`、`182_ros2_security_enclaves.md`、`183_ros2_threat_model.md`、`100_ament.md`、`101_build_tool.md`、`cross_compilation_build_tools_design.md`、`discovery_and_negotiation.md` | 第 5、7 批 |

设计文档里有意不收：`080_ros_documentation_system.md`、`per_package_documentation.md`（写文档系统）、`200_migration_guide_from_ros1.md`（ROS 1 迁移）、`114_generated_interfaces_python.md`（Python）。

## 有意不收（需要时告诉我再补）

| 源文档 | 原因 |
|---|---|
| `Get-Started/Installation/*`（除 RMW 部分）、`Get-Started/Introducing-Turtlesim/*`、`First-Steps.rst` | 安装步骤和入门操作，你已经具备 |
| `Migration-and-Upgrades/*` | ROS 1 迁移，与当前目标无关 |
| `The-ROS2-Project/*`、`Releases/*`、`Citations.rst`、`Contact.rst` | 社区、发布说明、贡献流程 |
| `Developer-Tools/Build/Releasing/*`、`Building-a-Custom-Deb-Package.rst`、`Core-maintainer-guide.rst`、`Package-Docs.rst`、`Package-documentation/*` | 发布到 ROS 官方仓库、写包文档 |
| `Developer-Tools/Build/*Eclipse*` | 特定 IDE |
| `Integrations-and-related/*`（IBM Cloud、Foxglove、Webots、MVSim、Gazebo 安装等） | 第三方集成与仿真器安装；仿真按需另开主题 |
| `Visualization/Rqtbag/Create-An-Rqtbag-Plugin.rst`、`middleware/Creating-An-RMW-Implementation.rst` | 写工具插件、写中间件实现，离日常开发太远 |
| `Writing-An-Async-Node-With-Asyncio-Python.rst` 等 Python 专属内容 | 以 C++ 为主 |
| `Capabilities/About-*.rst`（导航、感知、运动规划、仿真概览） | 只是指向 Nav2、MoveIt 等外部项目的概览，第 2 批之后可做一页"外部项目地图" |
