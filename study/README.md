# ROS 2 学习笔记（study/）

把本仓库的官方文档（`source/`）重组成"学会并能用"的形式，服务于移动机器人规划控制开发。

- **发行版：** ROS 2 Rolling
- **打开方式：** 用 Obsidian 把仓库根目录作为 vault 打开，或用 VS Code 打开仓库后预览 Markdown（VS Code 需装扩展 *Markdown Preview Mermaid Support* 才能显示图）
- **状态：** 样板阶段，只完成"执行器与回调组"一个主题

## 三层结构

| 层 | 位置 | 作用 |
|---|---|---|
| 1. 总览脑图 | [map/overview.md](map/overview.md) | 按用途找主题，链接到卡片 |
| 2. 概念卡片 | [cards/](cards/) | 每个概念一页，固定结构 |
| 3. 练习 | [labs/](labs/) | 可编译运行的小实验，先预测再运行 |

答案单独放在 [answers/](answers/)，避免做题时一眼看到。

## 卡片固定结构

1. 一句话定义
2. 为什么需要
3. 最小 C++ 代码（rclcpp，对应文件在 `labs/executor_lab/src/` 里，保证能编译）
4. 常见坑（结合导航/控制场景）
5. 与其他概念的关系（链接）
6. 2 道自测题（答案在 `answers/`）

## 来源标记

| 标记 | 含义 | 怎么用 |
|---|---|---|
| 📄 | 仓库文档原文的事实 | 可直接用；每张卡片顶部给出源文件链接 |
| 🔍 | 对照 rclcpp 源码或在 `ros:rolling` 镜像里实测核实过 | 可直接用；注明了核实日期和版本 |
| 💡 | 经验补充或推导 | 可信但要自己验证 |

## 本主题的学习路线（约 1.5 小时）

1. 读 [执行器](cards/executor.md)，合上页面答它的 2 道自测题，再对 [答案](answers/executor-answers.md)
2. 读 [回调组](cards/callback-group.md)，同样先答题再对 [答案](answers/callback-group-answers.md)
3. 做 [练习：控制环为什么被卡住](labs/executor-blocking.md)，**先填预测表再运行**
4. 选做练习末尾的挑战题
5. 一周后不看卡片，重做 4 道自测题；答不出的回卡片"常见坑"一节

## 约定

- 链接一律用相对路径的 Markdown 链接（不用 `[[wikilink]]`），Obsidian、VS Code、GitHub 都能跳转
- 不用标题锚点链接（`#某个标题`），因为 Obsidian 和 GitHub 生成锚点的规则不同
- 图用 Mermaid 的 `flowchart`（兼容性最好），不用 `mindmap`
