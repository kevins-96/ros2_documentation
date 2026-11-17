# 路线图：卡片与源文档对照

> 目的：说清楚每张卡片"吸收了哪些源文档"、哪些源文档有意不收。想核对原文时从这里跳。
> 路径都相对仓库的 `source/` 目录。✅ = 已完成。

## 编写约定（所有批次通用）

- 发行版 Rolling；C++ 代码在 `ros:rolling-ros-base` 镜像里编译实测，结果写进卡片或练习。
- 卡片固定六段：一句话定义 → 为什么需要 → 最小 C++ 代码 → 常见坑 → 与其他概念的关系 → 2 道自测题（答案在 `answers/`）。
- 来源标记：📄 文档原文 · 🔍 源码或实测核实 · 💡 经验补充。
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

## 第 3 批：让节点可配置可启停（配置与启停）

| 计划卡片 | 源文档 |
|---|---|
| 参数 | `ROS-Framework/About-Parameters.rst`、`parameters/Working-with-parameters.rst`、`Understanding-ROS2-Parameters/*`、`Using-ros2-param.rst`、`Using-Parameters-In-A-Class-CPP.rst`、`Monitoring-For-Parameter-Changes-CPP.rst` |
| 生命周期节点 | `nodes/Working-with-nodes/Managed-Nodes.rst` |
| launch 启动系统 | `Developer-Tools/About-Launch.rst`、`Launch/Launch-Main.rst`、`Launch-system.rst`、`Creating-Launch-Files.rst`、`Launch-file-different-formats.rst`、`Using-Substitutions.rst`、`Using-Event-Handlers.rst`、`Using-ROS2-Launch-For-Large-Projects.rst`、`nodes/.../Launching-Multiple-Nodes/*` |
| 启动参数与重映射 | `Launch/Node-arguments.rst`、`Get-Started/Configuring-ROS2-Environment.rst` |
| 日志 | `nodes/About-Logging/*`、`Learning-About-Logging/*`、`Using-Rqt-Console/*`、`Developer-Tools/Introspection-and-analysis/Logging-and-logger-configuration.rst` |
| 节点接口模板（可能并入节点卡片） | `nodes/Working-with-nodes/Using-Node-Interfaces-Template-Class.rst` |

## 第 4 批：让回调按时跑（并发与性能补全）

| 计划卡片 | 源文档 |
|---|---|
| ✅ 执行器、✅ 回调组 | `client-libraries/About-Executors/*`、`nodes/Working-with-nodes/Using-callback-groups.rst` |
| 组件与组合 | `nodes/About-Composition.rst`、`Composition.rst`、`Writing-a-Composable-Node.rst`、`Developer-Tools/Launch/Launching-composable-nodes.rst` |
| 进程内通信与零拷贝 | `nodes/Working-with-nodes/intra-process/*`、`Configure-ZeroCopy-loaned-messages.rst`、`interfaces/Working-with-interfaces/Buffer-Backends/*`（从第 2 批移来） |
| 实时编程 | `Capabilities/Motion-planning/Real-Time-Programming.rst`、`Allocator-Template-Tutorial.rst`、`Developer-Tools/Build/Building-Realtime-rt_preempt-kernel-for-ROS-2/*` |
| 客户端库内部结构（可选） | `About-Client-Libraries.rst`、`About-Internal-Interfaces/*` |

## 第 5 批：把代码变成能交付的包（工程化）

| 计划卡片 | 源文档 |
|---|---|
| 工作空间与 colcon | `Creating-A-Workspace/*`、`Colcon-Tutorial.rst`、`Creating-Your-First-ROS2-Package.rst`、`Developer-Tools/Build/Using-Variants.rst` |
| ament_cmake | `Developer-Tools/About-Build.rst`、`Build/About-Build-System.rst`、`Ament-CMake-Documentation.rst`、`Developing-a-ROS-2-Package.rst`、`Ament-Lint-For-Clean-Code.rst` |
| rosdep 依赖管理 | `Rosdep.rst`、`Developer-Tools/Build/Supplementing-Custom-Rosdep-Keys.rst` |
| pluginlib 插件 | `Pluginlib.rst`（Nav2 的规划器、控制器、代价地图层都是插件） |
| 测试 | `Developer-Tools/About-testing.rst`、`Debugging/About-testing.rst`、`Testing/Testing.rst`、`Testing/Cpp.rst`、`Testing/Integration.rst`、`Testing/CLI.rst` |

## 第 6 批：看见系统在干什么（调试与观测）

| 计划卡片 | 源文档 |
|---|---|
| 命令行工具 | `Introspection-and-analysis/About-Command-Line-Tools.rst`、`Getting-Started-With-Ros2doctor.rst`、`Publishing-Messages-Using-YAML-Files.rst`、`Examine-Traffic.rst` |
| rosbag 录制回放 | `Recording-And-Playing-Back-Data/*`、`Recording-A-Bag-From-Your-Own-Node-CPP.rst`、`Reading-From-A-Bag-File-CPP.rst`、`Testing/Overriding-QoS-Policies-For-Recording-And-Playback.rst` |
| 服务与动作内省 | `Introspection-and-analysis/Service-Introspection.rst`、`actions/Working-with-actions/Action-Introspection.rst` |
| 调试与回溯 | `Developer-Tools/About-Debugging.rst`、`Debugging/Getting-Backtraces-in-ROS-2.rst`、`Debugging/ROS-2-IDEs.rst` |
| tracing 性能追踪 | `About-introspection-and-analysis.rst`、`Tracing/ROS2-Tracing-Trace-and-Analyze.rst`、`Debugging/Building-ROS-2-with-Tracing.rst` |
| RViz 与可视化 | `About-visualization.rst`、`Visualization/RViz/*`（用户指南、Marker 三篇；自定义 Display/Panel 按需）、`About-RQt.rst` |

## 第 7 批：让多台机器连起来（中间件与部署）

| 计划卡片 | 源文档 |
|---|---|
| 中间件 RMW 与 DDS | `ROS-Framework/About-Middleware.rst`、`middleware/About-Middleware-Implementations.rst`、`About-Different-Middleware-Vendors.rst`、`Get-Started/Installation/RMW-Implementations/*`（含 Zenoh） |
| Domain ID 与发现 | `nodes/About-Domain-ID.rst`、`Launch/Improved-Dynamic-Discovery.rst`、`Launch/Discovery-Server/*` |
| DDS 调优 | `Developer-Tools/Build/DDS-tuning.rst`、`Launch/FastDDS-Configuration.rst` |
| Docker 部署 | `Build/Run-2-nodes-in-single-or-separate-docker-containers.rst`、`Setup-ROS-2-with-VSCode-and-Docker-Container.rst`、`Deployment-Guidelines/*`、`Build/About-Cross-Compilation.rst`、`Cross-compilation.rst` |
| 安全 SROS2 | `Launch/Security/*`、`Build/The-Keystore.rst` |

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
