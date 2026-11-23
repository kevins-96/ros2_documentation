---
tags: [ros2, 设计理念, 安全, 威胁模型, STRIDE, DREAD]
distro: rolling
status: design-batch-11
aliases: [威胁模型, Threat Model, 机器人威胁模型, STRIDE, DREAD, 信任边界, trust boundary]
---

# 威胁模型：机器人系统要防什么，SROS2 能挡住哪些

> 适用：ROS 2 **Rolling**（2026-11-23 对照 `ros:rolling-ros-base`、sros2 0.17.1、launch 实测）
> 来源标记：📐 设计文档原文 · 📄 本仓库文档原文 · 🔍 源码/实测核实 · 💡 经验补充
> 设计文档（[ros2/design](https://github.com/ros2/design) @ `93a415b`，本地副本在 `external/ros2_design/articles/`）：
> [ROS 2 Robotic Systems Threat Model](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/183_ros2_threat_model.md)（2019-03，2021-01 修订）。原文很长（5000 多行），主要是两张大表：通用威胁表，以及 TurtleBot 3 和 MARA 机械臂两个实例分析
> 本篇不讲新的 API，讲的是**怎么系统地想安全问题**。前置：[安全的设计](security-design.md)、[访问控制策略的设计](access-control-design.md)

## 1. 设计要回答的问题

开了 SROS2 就安全了吗？设计文档的回答是：先列清楚**谁会攻击**、**要保护什么**、**从哪里进来**，再看每一种威胁由谁来挡 📐。很多威胁 DDS-Security 根本管不到，比如 SD 卡被拔走、SSH 用默认密码、第三方节点故意输出错误数据。

文档第一部分故意和 ROS 无关，讲"组件化架构的机器人"通用的威胁；第二部分才落到 ROS 2 节点 📐。

## 2. 考虑过的方案与取舍

### 三个基本概念 📐

| 概念 | 定义 | 机器人里的例子 |
|---|---|---|
| 参与者（actors） | 和机器人交互的人或外部系统 | 操作员、维护人员、云服务、旁边的行人 |
| 资产（assets） | 要保护的用户、资源或属性 | 人身安全、机器人本体不损坏、传感器数据的隐私、CPU 和磁盘、凭据 |
| 入口（entry points） | 系统和外界交互的方式 | DDS 话题、Wi-Fi、蓝牙、SSH、USB 传感器、部署服务 |

### 信任边界：可信和不可信组件 📐

- **可信组件**：由应用的所有者（或审查过的伙伴）开发、构建、测试、部署，可以假设它遵守规格。但可信组件也可能泄露数据。
- **不可信组件**：第三方节点。它可能是恶意的，质量保证也可能不如自家代码；发布流程本身也可能被攻破。
- 规则：**可信组件不应信任不可信组件；不可信组件之间也不应互相信任** 📐。

设计文档举的例子很贴近规划控制：一个第三方的目标跟踪节点，机器人按它估计的目标位置跟随，速度和距离成正比 📐：

- 隐私：它把视频偷偷传出去。
- 人身安全：它故意把目标位置报得很远，让机器人突然加速撞人。
- 可用性：它占满 CPU、内存、磁盘。
- 完整性：它控制"检测到的目标在哪"，等于在遥控机器人。

还有不那么显然的入口：URDF 存在文件系统里，里面有关节限位；文件系统被改了，限位就没了 📐。

### STRIDE 分类 + DREAD 打分 📐

每个威胁先按 STRIDE 归类：

| 字母 | 威胁 | 机器人里的例子 |
|---|---|---|
| S（Spoofing） | 冒充身份 | 冒充 `joy_node` 发 `/cmd_vel` |
| T（Tampering） | 篡改 | 中间人改写 `/cmd_vel` |
| R（Repudiation） | 抵赖 | 事后查不出是谁发的指令 |
| I（Information disclosure） | 信息泄露 | 偷听相机话题、拷走日志 |
| D（Denial of service） | 拒绝服务 | 往话题里狂发消息、干扰 Wi-Fi、耗电 |
| E（Elevation of privilege） | 提权 | 第三方节点拿到别的节点的凭据 |

再用 DREAD 打分：伤害（Damage）、可复现性（Reproducibility）、可利用性（Exploitability）、受影响用户（Affected users）、可发现性（Discoverability）五项，每项 1–3 分（1 最轻，3 最重）📐。总分

$$
\text{DREAD} = D + R + E + A + D_{\text{isc}} \in [5,\ 15]
$$

💡 DREAD 是经验打分，不同人打出来的分会不一样。它的价值在于强迫团队对每一条威胁讨论"多严重、多容易"，再按分数排优先级。

### 通用威胁表里的高分项（节选）📐

| 威胁 | DREAD | 文档给的缓解思路 |
|---|---|---|
| 篡改组件之间传的消息 | 15 | 认证 + 加密（SROS2） |
| 未授权写入通信通道 | 15 | 访问控制 |
| 让通信通道不可用（刷屏） | 15 | SROS2 能限制"谁能发"，但挡不住**允许发的节点**刷屏；需要限流 |
| 未授权修改配置 | 15 | 参数可以被任何参与者改 |
| 用户自己配错 | 15 | 配置变更可回滚、原子化 |
| 物理接触修改文件系统、调试口、内部总线 | 15 | 加密文件系统、封装机箱 |
| 部署了恶意节点 | 14 | 每个节点用不同的系统用户运行，限制 CPU、磁盘配额 |
| 某个节点发出错误数据 | 13 | 校验收到的消息；越界的关节指令要处理，但别动不动触发急停 |
| 阻止某个组件运行（例如杀进程） | 13 | 进程退出后自动重启 |
| 冒充身份 | 10 | 认证 |

### TurtleBot 3 实例（节选）📐

文档把通用威胁逐条套到 TurtleBot 3 上，表里有一列"开了 SROS 吗"，还有一列"缓解结果"（重新设计 / 转移 / 避免 / 接受）。几条和导航最相关的：

- **没开 SROS 时，任何节点可以用任何名字，冒充是"轻而易举"的**；开了之后风险降低。
- **恶意节点不开 DDS-Security 混进来**：governance 里 `allow_unauthenticated_participants` 要设为 `false`。
- **偷到凭据后发 `/cmd_vel`**：`permissions.xml` 要尽量收紧，风险"转移给配置权限的人"；文档还说权限文件最好自动生成，手写容易错。
- **参与者被枚举、指纹识别**：governance 的 `metadata_protection_kind` 要设成加密。
- **允许通信的节点刷屏**：需要限流，"恶意节点怎么强制执行？观察然后杀掉？"仍是开放问题。
- **节点意外发出很大或随机的摇杆值**：建议扩展 IDL，让用户写数据的有效范围，自动校验；文档自己说这需要"大量工程"。
- **恶意 `joy_node` 随机杀掉其他节点**：launch 里设退出处理器自动重启。
- **参数可以被任何 DDS 参与者读和改**。

最后给出攻击树：攻击者部署恶意节点 → 偷凭据 / 刷屏 / 杀进程 / 改配置，以及代码仓库被攻破的路径 📐。

## 3. 设计与 Rolling 现状对照

状态：✅ 已有机制 · 🔄 机制不同 · ⏳ 未实现 · 🗑️ 已过时。🔍 结果来自[练习](../labs/security-build-design-check.md)第 1–3 部分和第 7 批的安全练习。

| 威胁 / 建议的缓解 | 状态 | Rolling 实测 |
|---|---|---|
| 认证、加密防冒充和篡改 | ✅ | 开安全后抓包 0 个明文，没证书的节点连发布者都看不见 🔍（第 7 批） |
| `allow_unauthenticated_participants=false` | ✅ 默认就是 | 默认 governance 里是 `false`，发现、存活性、数据、元数据都加密 🔍（第 7 批） |
| 限制谁能发 `/cmd_vel` | ✅（按进程） | 访问控制在创建发布者时就拒绝 🔍。但同一 enclave 里的节点权限合并，挡不住**同进程**的恶意组件 🔍（见[访问控制策略的设计](access-control-design.md)） |
| 参数可被任何参与者修改 | 🔄 取决于 policy | 参数服务就是普通服务，受访问控制约束 🔍。但 `create_enclave` 生成的**默认**权限允许所有 `rq/*`、`rr/*`，也就是任何 enclave 都能改任何节点的参数 🔍（第 7 批）。要自己写 policy 收紧 |
| 允许发的节点刷屏，需要限流 | ⏳ | ROS 2 没有发布端限流的 QoS 💡。DDS-Security 不管频率 📐 |
| IDL 里写数据有效范围，自动校验 | ⏳ | `.msg` 只能限制字符串和数组的长度上限 📄（见卡片 [接口定义](../cards/communication/interface-definition.md)），没有数值范围约束。校验要自己写 💡 |
| 被杀掉的节点自动重启 | 🔄 换了名字 | 设计文档写的是 `restart_exit_handler`（早期 launch 的接口）；Rolling 的 launch 用 `ExecuteProcess`/`Node` 的 `respawn`、`respawn_delay`、`respawn_max_retries` 参数 🔍 |
| 审计"谁做了什么"（防抵赖） | 🔄 部分 | DDS-Security 日志插件可以把拒绝事件写到文件（`ROS_SECURITY_LOG_FILE`）🔍。数据样本本身不带签名者信息 💡 |
| 每个节点用不同系统用户运行、限制资源 | ⏳ ROS 不管 | 💡 交给操作系统：systemd 单元的 `User=`、`CPUQuota=`、`MemoryMax=`，或者容器的资源限制 |
| 加密文件系统、保护私钥 | ⏳ ROS 不管 | 🔍 容器里以 root 创建的 `key.pem` 是 `-rw-rw-rw-`（第 7 批）。💡 部署时 `chmod 600`，私钥最好放进 TPM 或安全芯片 |
| 加密后旁观者看不到"谁在网络上" | ⏳ | 抓包仍能看到 enclave 名字（证书是明文）🔍（第 7 批） |

## 4. 对写代码的影响（规划控制场景）

1. **把"控制指令的来源"当成不可信输入。** 设计文档的跟踪节点例子说明：攻击不一定要破解加密，只要让一个被信任的节点输出看似合法的错误数据就够了 📐。💡 控制器里对速度、加速度、曲率做限幅，对位姿跳变做检查；越界时降级（减速、停止跟随），而不是直接急停（文档特别提到别让攻击者轻易触发急停 📐）。
2. **按信任边界划分进程和 enclave。** 第三方节点单独进程、单独 enclave、单独系统用户 💡。见[安全的设计](security-design.md)第 4 节。
3. **用 DREAD 给自己的系统排个序。** 💡 列出你的 `/cmd_vel`、`/map`、`/initialpose`、参数服务、SSH、OTA 更新，每项打一遍分，先处理 13 分以上的。
4. **`/initialpose`、`/goal_pose` 这类"一条消息就改变行为"的话题要单独收紧。** 💡 默认权限谁都能发；一条伪造的 `/initialpose` 就能让定位跳到错误位置。
5. **关键进程设 `respawn`，但要限次数。** `respawn_max_retries` 默认是负数，表示无限重启 🔍。💡 一个启动即崩溃的节点会被无限拉起，日志刷满磁盘，本身就是一种拒绝服务。

## 5. 与其他概念的关系

- 卡片：[安全 SROS2](../cards/middleware/sros2-security.md) · [launch](../cards/configuration/launch.md)（`respawn`）· [参数](../cards/configuration/parameter.md) · [接口定义](../cards/communication/interface-definition.md)
- 练习：[SROS2 密钥库、强制模式与访问控制](../labs/sros2-keystore.md) · [安全与工程化的设计还成立吗](../labs/security-build-design-check.md)
- [安全的设计](security-design.md) · [访问控制策略的设计](access-control-design.md)
- [QoS 的设计](qos-design.md)：DEADLINE、LIVELINESS 能发现"节点没了"，但不能防刷屏
- 上一级：[总览脑图](../map/overview.md)

## 6. 自测题（先自己答，再看[答案](../answers/threat-model-design-answers.md)）

1. 你的 AMR 上有一个第三方的"动态障碍物预测"节点，输出行人未来 2 秒的轨迹，局部规划器据此避让。按设计文档的思路：（a）这个节点是可信还是不可信组件？（b）用 STRIDE 列出它可能造成的至少三类威胁，各举一个具体后果。（c）开了 SROS2、权限也收紧了，哪些威胁仍然存在？你在规划器这一侧能做什么？
2. 给下面两条威胁各打一遍 DREAD 分（每项 1–3），写出理由，并说明哪一条应该先处理：（a）同一 Wi-Fi 下的人用 `ros2 topic pub /initialpose ...` 让机器人定位跳到别处（系统没开 SROS2）。（b）攻击者拿到机器人 SD 卡，拷走里面的地图和日志。
