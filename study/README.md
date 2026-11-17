# ROS 2 学习笔记（study/）

把本仓库的官方文档（`source/`）重组成"学会并能用"的形式，服务于移动机器人规划控制开发。

- **发行版：** ROS 2 Rolling
- **打开方式：** 用 Obsidian 把仓库根目录作为 vault 打开，或用 VS Code 打开仓库后预览 Markdown（VS Code 需装扩展 *Markdown Preview Mermaid Support* 才能显示图）
- **进度：** 见 [路线图](ROADMAP.md)。已完成：通信（第 1 批）、执行器与回调组（样板）

## 三层结构

| 层 | 位置 | 作用 |
|---|---|---|
| 1. 总览脑图 | [map/overview.md](map/overview.md) | 按用途找主题，链接到卡片 |
| 2. 概念卡片 | [cards/](cards/)，按用途分子目录 | 每个概念一页，固定结构 |
| 3. 练习 | [labs/](labs/) | 可编译运行的小实验，先预测再运行 |

答案单独放在 [answers/](answers/)，避免做题时一眼看到。

```text
study/
├── README.md            本页
├── ROADMAP.md           每张卡片吸收了哪些源文档、哪些有意不收
├── map/overview.md      总览脑图 + 链接清单
├── cards/
│   ├── communication/   让模块互相说话
│   └── concurrency/     让回调按时跑
├── answers/             自测题答案（xxx-answers.md）
└── labs/
    ├── *.md             练习说明
    ├── study_interfaces/  自定义 msg/srv/action（练习共用）
    ├── comm_lab/          通信练习与卡片示例代码
    └── executor_lab/      执行器练习与卡片示例代码
```

## 卡片固定结构

1. 一句话定义
2. 为什么需要
3. 最小 C++ 代码（rclcpp，对应源文件在 `labs/*/src/` 里，保证能编译）
4. 常见坑（结合导航/控制场景）
5. 与其他概念的关系（链接）
6. 2 道自测题（答案在 `answers/`）

## 来源标记

| 标记 | 含义 | 怎么用 |
|---|---|---|
| 📄 | 仓库文档原文的事实 | 可直接用；每张卡片顶部给出源文件链接 |
| 🔍 | 对照 rclcpp 源码或在 `ros:rolling-ros-base` 镜像里实测核实过 | 可直接用；注明了核实日期和版本 |
| 💡 | 经验补充或推导 | 可信但要自己验证 |

## 编译所有练习

```bash
mkdir -p ~/study_ws/src
ln -s <你的仓库路径>/study/labs/study_interfaces <你的仓库路径>/study/labs/comm_lab \
      <你的仓库路径>/study/labs/executor_lab ~/study_ws/src/
cd ~/study_ws && source /opt/ros/rolling/setup.bash
colcon build && source install/setup.bash
```

或用官方镜像：`docker run -it --rm -v <你的仓库路径>/study/labs:/ws/src ros:rolling-ros-base`，容器内 `cd /ws && colcon build`。

## 建议学习路线

每个主题都按同一个节奏：**读卡片 → 合上页面答 2 道自测题 → 对答案 → 做练习（先填预测表）→ 一周后不看卡片重做自测题**。

1. **通信**：[节点](cards/communication/node.md) → [通信方式怎么选](cards/communication/communication-patterns.md) → [话题](cards/communication/topic.md) → [QoS](cards/communication/qos.md)（练习：[QoS 不匹配](labs/qos-mismatch.md)）→ [服务](cards/communication/service.md) → [动作](cards/communication/action.md)（练习：[动作抢占](labs/action-preempt.md)）→ [接口定义](cards/communication/interface-definition.md) → [话题进阶](cards/communication/topic-advanced.md)（选读）
2. **并发**：[执行器](cards/concurrency/executor.md) → [回调组](cards/concurrency/callback-group.md)（练习：[控制环为什么被卡住](labs/executor-blocking.md)）
3. 之后的主题按 [路线图](ROADMAP.md) 补齐

## 约定

- 链接一律用相对路径的 Markdown 链接，Obsidian、VS Code、GitHub 都能跳转
- 全库文件名不重名，以后要改成 `[[wikilink]]` 可以按文件名直接替换
- 不用标题锚点链接（`#某个标题`），因为 Obsidian 和 GitHub 生成锚点的规则不同
- 总览图用 Mermaid `mindmap`；卡片里的流程图用 `flowchart` 或 `sequenceDiagram`
