# ROS 2 学习笔记（study/）

把本仓库的官方文档（`source/`）重组成"学会并能用"的形式，服务于移动机器人规划控制开发。

- **发行版：** ROS 2 Rolling
- **打开方式：** 用 Obsidian 把仓库根目录作为 vault 打开，或用 VS Code 打开仓库后预览 Markdown（VS Code 需装扩展 *Markdown Preview Mermaid Support* 才能显示图）
- **进度：** 见 [路线图](ROADMAP.md)。已完成：通信（第 1 批）、坐标与机器人模型（第 2 批）、配置与启停（第 3 批）、并发与实时性（样板 + 第 4 批）、工程化（第 5 批）、调试与观测（第 6 批）、中间件与部署（第 7 批）、设计理念 · 通信与中间件（第 8 批）

## 四层结构

| 层 | 位置 | 作用 |
|---|---|---|
| 1. 总览脑图 | [map/overview.md](map/overview.md) | 按用途找主题，链接到卡片 |
| 2. 概念卡片 | [cards/](cards/)，按用途分子目录 | 每个概念一页，固定结构 |
| 3. 练习 | [labs/](labs/) | 可编译运行的小实验，先预测再运行 |
| 4. 设计理念 | [design/](design/) | 读 [ros2/design](https://github.com/ros2/design) 设计文档后写的笔记：为什么这样设计、当时考虑过哪些方案、和 Rolling 现状对比。建议学完对应卡片再读 |

答案单独放在 [answers/](answers/)，避免做题时一眼看到。

```text
study/
├── README.md            本页
├── ROADMAP.md           每张卡片吸收了哪些源文档、哪些有意不收
├── map/overview.md      总览脑图 + 链接清单
├── cards/
│   ├── communication/   让模块互相说话
│   ├── coordinates/     知道东西在哪
│   ├── configuration/   让节点可配置可启停
│   ├── concurrency/     让回调按时跑
│   ├── engineering/     把代码变成能交付的包
│   ├── observability/   看见系统在干什么
│   └── middleware/      让多台机器连起来
├── design/              设计理念笔记（固定结构见下）
├── answers/             自测题答案（xxx-answers.md）
└── labs/
    ├── *.md             练习说明
    ├── study_interfaces/  自定义 msg/srv/action（练习共用；FixedGrid 用于第 8 批的消息初始化实测）
    ├── comm_lab/          通信练习与卡片示例代码
    ├── tf_lab/            坐标变换练习、卡片示例代码与 URDF 模型
    ├── config_lab/        参数、生命周期、launch、日志练习与卡片示例代码
    ├── perf_lab/          组件、进程内通信、借用消息、实时循环练习
    ├── build_lab/         工程化练习：path_smoother_core（库、插件基类、节点、测试）+ path_smoother_plugins（插件）
    ├── observe_lab/       调试与观测练习：fake_planner（被观察的假规划器）、bag 读写、图查询、回调时长统计脚本
    ├── mw_lab/            中间件与部署练习：mw_probe（连通性探针）、序列化基准、Fast DDS XML、SROS2 策略、compose 文件
    ├── design_lab/        设计理念实测：名字映射、参与者、QoS 语义、动作结果缓存、消息初始化基准、DDS 抓包脚本
    └── executor_lab/      执行器练习与卡片示例代码
```

## 卡片固定结构

1. 一句话定义
2. 为什么需要
3. 最小 C++ 代码（rclcpp，对应源文件在 `labs/*/src/` 里，保证能编译）
4. 常见坑（结合导航/控制场景）
5. 与其他概念的关系（链接）
6. 2 道自测题（答案在 `answers/`）

## 设计笔记固定结构

1. 设计要回答的问题
2. 考虑过的方案与取舍（为什么选了现在这个）
3. 设计文档 vs Rolling 现状对照表（✅ 按设计实现 · 🔄 实现与设计不同 · ⏳ 未实现 · 🗑️ 已过时）
4. 对写代码的影响（规划控制场景）
5. 与其他概念的关系（卡片、练习）
6. 2 道自测题（答案在 `answers/`）

设计文档很多是早期提案，写完后实现又改过。**以对照表和 🔍 为准，不要直接按设计文档写代码。**

## 来源标记

| 标记 | 含义 | 怎么用 |
|---|---|---|
| 📄 | 仓库文档原文的事实 | 可直接用；每张卡片顶部给出源文件链接 |
| 🔍 | 对照 rclcpp 源码或在 `ros:rolling-ros-base` 镜像里实测核实过 | 可直接用；注明了核实日期和版本 |
| 💡 | 经验补充或推导 | 可信但要自己验证 |
| 📐 | [ros2/design](https://github.com/ros2/design) 设计文档原文（固定在提交 `93a415b`） | 讲"为什么"，不一定是现状；和 Rolling 不同的地方在对照表里标了 🔄、⏳ 或 🗑️ |

## 编译所有练习

```bash
mkdir -p ~/study_ws/src
ln -s <你的仓库路径>/study/labs/study_interfaces <你的仓库路径>/study/labs/comm_lab \
      <你的仓库路径>/study/labs/executor_lab <你的仓库路径>/study/labs/tf_lab \
      <你的仓库路径>/study/labs/config_lab <你的仓库路径>/study/labs/perf_lab \
      <你的仓库路径>/study/labs/build_lab <你的仓库路径>/study/labs/observe_lab \
      <你的仓库路径>/study/labs/mw_lab <你的仓库路径>/study/labs/design_lab ~/study_ws/src/
cd ~/study_ws && source /opt/ros/rolling/setup.bash
colcon build && source install/setup.bash
```

或用官方镜像：`docker run -it --rm -v <你的仓库路径>/study/labs:/ws/src ros:rolling-ros-base`，容器内 `cd /ws && colcon build`。

## 建议学习路线

每个主题都按同一个节奏：**读卡片 → 合上页面答 2 道自测题 → 对答案 → 做练习（先填预测表）→ 一周后不看卡片重做自测题**。

1. **通信**：[节点](cards/communication/node.md) → [通信方式怎么选](cards/communication/communication-patterns.md) → [话题](cards/communication/topic.md) → [QoS](cards/communication/qos.md)（练习：[QoS 不匹配](labs/qos-mismatch.md)）→ [服务](cards/communication/service.md) → [动作](cards/communication/action.md)（练习：[动作抢占](labs/action-preempt.md)）→ [接口定义](cards/communication/interface-definition.md) → [话题进阶](cards/communication/topic-advanced.md)（选读）
2. **坐标**：[tf2 坐标变换](cards/coordinates/tf2.md) → [四元数](cards/coordinates/quaternion.md)（练习：[四元数与 tf2 缓冲](labs/quaternion-buffer.md)）→ [tf2 与时间](cards/coordinates/tf2-time.md)（练习：[lookupTransform 的时间参数](labs/tf-time-lookup.md)）→ [URDF 与机器人模型](cards/coordinates/urdf.md)（练习：[URDF 关节与 TF](labs/urdf-joint-states.md)）
3. **配置与启停**：[参数](cards/configuration/parameter.md)（练习：[参数的规则与陷阱](labs/parameter-rules.md)）→ [启动参数与重映射](cards/configuration/node-arguments.md) → [launch 启动系统](cards/configuration/launch.md)（练习：[launch 与名字解析](labs/launch-and-names.md)）→ [日志](cards/configuration/logging.md) → [生命周期节点](cards/configuration/lifecycle-node.md)（练习：[生命周期状态切换](labs/lifecycle-transitions.md)）
4. **并发与实时性**：[执行器](cards/concurrency/executor.md) → [回调组](cards/concurrency/callback-group.md)（练习：[控制环为什么被卡住](labs/executor-blocking.md)）→ [组件与组合](cards/concurrency/component.md)（练习：[组件与容器](labs/component-container.md)）→ [进程内通信与零拷贝](cards/concurrency/intra-process.md)（练习：[进程内通信与零拷贝](labs/intra-process-zero-copy.md)）→ [实时编程](cards/concurrency/realtime.md)（练习：[实时循环的抖动](labs/realtime-jitter.md)）
5. **工程化**：[工作空间与 colcon](cards/engineering/workspace-colcon.md) → [ament_cmake](cards/engineering/ament-cmake.md) → [rosdep 依赖管理](cards/engineering/rosdep.md)（练习：[编译与依赖的常见错误](labs/build-pitfalls.md)）→ [pluginlib 插件](cards/engineering/pluginlib.md)（练习：[用插件换路径平滑器](labs/plugin-smoother.md)）→ [测试](cards/engineering/testing.md)（练习：[测试与代码规范检查](labs/testing-and-lint.md)）
6. **调试与观测**：[命令行工具](cards/observability/cli-tools.md) → [服务与动作内省](cards/observability/service-introspection.md)（练习：[命令行与服务内省](labs/cli-and-introspection.md)）→ [rosbag 录制回放](cards/observability/rosbag.md) → [RViz 与可视化](cards/observability/rviz.md)（练习：[录制、回放与离线分析](labs/bag-record-replay.md)）→ [调试与回溯](cards/observability/gdb-debugging.md) → [tracing 性能追踪](cards/observability/tracing.md)（练习：[崩溃回溯与回调追踪](labs/crash-and-trace.md)）
7. **中间件与部署**：[中间件 RMW 与 DDS](cards/middleware/rmw-dds.md) → [Domain ID 与发现](cards/middleware/domain-discovery.md)（练习：[中间件、Domain 与发现](labs/middleware-discovery.md)）→ [DDS 调优与 Fast DDS XML](cards/middleware/dds-tuning.md) → [Docker 部署](cards/middleware/docker-deploy.md)（练习：[DDS 调优、XML 配置与容器网络](labs/dds-and-docker.md)）→ [安全 SROS2](cards/middleware/sros2-security.md)（练习：[SROS2 密钥库、强制模式与访问控制](labs/sros2-keystore.md)）
8. **设计理念**（学完 1 和 7 之后读）：[为什么有 ROS 2、为什么建在 DDS 上](design/why-ros2-dds.md) → [ROS 图怎么映射到 DDS](design/graph-to-dds-mapping.md) → [QoS 的设计](design/qos-design.md) → [服务与动作的设计](design/service-action-design.md) → [接口定义的设计](design/interface-design.md)（练习：[设计文档的说法在 Rolling 里还成立吗](labs/design-claims-check.md)）
9. 文档原有内容的七批已全部完成；设计理念的后续批次和有意不收的文档见[路线图](ROADMAP.md)

## 约定

- 链接一律用相对路径的 Markdown 链接，Obsidian、VS Code、GitHub 都能跳转
- 全库文件名不重名，以后要改成 `[[wikilink]]` 可以按文件名直接替换
- 不用标题锚点链接（`#某个标题`），因为 Obsidian 和 GitHub 生成锚点的规则不同
- 总览图用 Mermaid `mindmap`；卡片里的流程图用 `flowchart` 或 `sequenceDiagram`
