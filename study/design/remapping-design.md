---
tags: [ros2, 设计理念, 重映射, 命令行参数]
distro: rolling
status: design-batch-9
aliases: [名字与重映射的设计, 重映射设计, Remapping Names, ROS Command Line Arguments, ros-args 设计]
---

# 名字与重映射的设计：规则怎么匹配、为什么要 `--ros-args`

> 适用：ROS 2 **Rolling**（2026-11-23 在 `ros:rolling-ros-base`、rclcpp 33.2.0、rcl 10.6.0、launch_ros 0.30.3 上实测）
> 来源标记：📐 设计文档原文 · 📄 本仓库文档原文 · 🔍 源码/实测核实 · 💡 经验补充
> 设计文档（[ros2/design](https://github.com/ros2/design) @ `93a415b`，本地副本在 `external/ros2_design/articles/`）：
> [Remapping Names](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/141_static_remapping.md)（2017-03，2020-03 修订）、
> [ROS Command Line Arguments](https://github.com/ros2/design/blob/93a415bf928751d37fce6f83215c521658e20c93/articles/160_ros_command_line_arguments.md)（2019-09，2021-08 修订）
> 先读卡片 [启动参数与重映射](../cards/configuration/node-arguments.md)（`-r`、`__ns`、`__node` 的写法和常见坑）。名字本身的规则见 [ROS 图怎么映射到 DDS](graph-to-dds-mapping.md)。

## 1. 设计要回答的问题

重映射（remapping）让同一个可执行文件能用在系统的不同位置 📐。例如两个同型号的激光雷达跑同一个驱动，一个输出到 `/head_scan`，另一个输出到 `/base_scan`。ROS 2 在 ROS 1 的基础上多了几个难点 📐：

- **一个进程里可以有多个节点**。规则要能只作用于其中一个节点，否则会把同进程的其他节点也改掉。
- **ROS 1 的规则只能做全名精确替换**。actionlib 一个动作有 5 个话题，重映射一个动作就要写 5 条规则。
- **命令行上既有重映射又有参数赋值**。两者都用 `:=`，而 ROS 2 用前导下划线表示隐藏名字，`__ns` 这类特殊名字容易和普通名字混淆。用户自己的程序参数也会和 ROS 的参数冲突。

## 2. 考虑过的方案与取舍

### 重映射要支持的用例 📐

设计文档列了 11 个用例，提出的语法支持其中 9 个：

| 用例 | 例子 | 设计提出的语法 |
|---|---|---|
| 只改进程里的一个节点 | `node1:scan:=scan_filtered` | `节点名:` 前缀 |
| 改命名空间（一条规则改掉 actionlib 的 5 个话题） | `/foo/**:=/fizz/\1` | 通配符 `*`（一个 token）、`**`（零个或多个 token）+ 反向引用 `\1`–`\9` |
| 改末尾的名字（basename） | `**/scan:=\1/scan_filtered` | 同上 |
| 全名、相对名精确替换（和 ROS 1 一样） | `/ns/bar:=/ns/foo`、`bar:=foo` | 两边都先展开成全名再比较 |
| 改默认命名空间、改节点名 | `__ns:=/robot1`、`__node:=left_driver`（也可写 `__name`） | 特殊规则，可以加节点前缀 |
| 话题和服务分开改 | `rostopic://map:=map_stream` | URL 前缀 |
| 改所有位置上的某个 token（例如把厂商名去掉） | — | **不支持**，可以用两条通配规则凑出来 |
| 按代码里写的名字匹配（展开成全名之前） | — | **不支持**，没有替代办法 |

**规则的应用顺序** 📐：先改节点名，再改命名空间，最后是其他规则；同一类按用户给出的顺序。一个名字被前面的规则改过之后，就按新名字去匹配后面的规则。

**静态与动态重映射** 📐：现在只做启动时的静态重映射。如果以后做运行时的动态重映射，它应该排在静态规则之后，因为用户在内省工具里看到的是静态重映射之后的名字。

### 命令行为什么用 `--ros-args`（160）📐

| 方案 | 结论 |
|---|---|
| 换一个运算符（参数用 `:=`、重映射用 `~=`） | 不用。写起来短，但容易写错 |
| 用 URL 区分（`rosparam://a:=b`、`rosremap://a:=b`） | 不用。太啰嗦 |
| 每个选项都加 `--ros-` 前缀（`--ros-remap`） | 不用。解析时要事先知道全部选项，也啰嗦 |
| **用 `--ros-args ... --` 把 ROS 参数圈起来，里面用 `-r`、`-p` 这类选项** | **采用**。大家熟悉选项的写法，不容易出错，还能 Tab 补全。可以出现多段 `--ros-args`，不同来源（例如 launch 和用户）各自追加，互不干扰 |

分工是 rcl 抽出 `--ros-args` 里的内容，解析自己认识的选项；不认识的留给上层客户端库，用户自己的参数仍然能拿到 📐。

## 3. 设计与 Rolling 现状对照

状态：✅ 按设计实现 · 🔄 实现与设计不同 · ⏳ 未实现 · 🗑️ 已过时。🔍 结果来自[练习](../labs/config-design-check.md)第 1 部分（`remap_probe`：一个进程里有 `/robot1/planner` 和 `/robot1/controller` 两个节点）。

| 设计文档的说法 | 状态 | Rolling 实测 |
|---|---|---|
| 不带前缀的规则作用于进程里的所有节点 | ✅ | `-r scan:=scan_filtered` 同时改了 planner 和 controller 🔍 |
| `节点名:` 前缀只改一个节点 | ✅ | `-r planner:scan:=scan_filtered` 只改 planner 🔍。前缀只能写节点名，不能带命名空间：`/robot1/planner:scan:=x` 解析报错 🔍 |
| 通配符 `*`、`**` 和反向引用 `\1` | ⏳ | 解析报错，节点起不来：`Wildcard '**' is not implemented`、`Wildcard '*' is not implemented` 🔍。参数 YAML 的键支持通配符，重映射不支持 |
| `rostopic://`、`rosservice://` 让规则只作用于话题或服务 | ✅ | `map` 同时是话题和服务，`-r rostopic://map:=map_stream` 只改话题，`-r rosservice://map:=map_srv` 只改服务 🔍。替换的一侧不能带 URL，否则解析报错 🔍 |
| 一条规则改掉一个动作的全部话题 | ✅ 换了一种方式 | 不靠通配符，而是动作名本身参与重映射：`-r move_to:=move_to_checked` 后，3 个隐藏服务和 2 个隐藏话题都变成 `move_to_checked/_action/...` 🔍 |
| — | 🔍 新发现 | 动作的**单个隐藏话题**也能被重映射：`-r /robot1/move_to/_action/status:=/x` 后，status 话题真的变成了 `/x`，动作的其余部分不变。客户端就找不到 status 了 💡 |
| `__node` 和 `__name` 都能改节点名 | ✅ | 两者效果一样 🔍 |
| 先改节点名、再改命名空间 | ✅ | `planner:__ns:=/a` 和 `planner:__node:=p2` 一起给（不论顺序），节点变成 `/robot1/p2`，命名空间规则没生效，因为改名后它已经不叫 planner 了。写成 `planner:__node:=p2` 和 `p2:__ns:=/a` 才得到 `/a/p2` 🔍 |
| 带节点前缀的 `__ns` 和全局的 `__ns` 冲突时，先给的生效 | ✅ | `planner:__ns:=/foo` 在前、`__ns:=/bar` 在后：planner 在 `/foo`，controller 在 `/bar` 🔍 |
| ROS 1 用环境变量 `ROS_NAMESPACE` 改默认命名空间 | 🗑️ ROS 2 不支持 | 设置 `ROS_NAMESPACE=/envns` 没有任何效果 🔍 |
| 规则匹配全名：相对规则先按节点命名空间展开 | ✅ | `-r /scan/head/scan:=fizz` 把绝对名改成了 `/robot1/fizz`：替换侧的相对名按节点命名空间展开 🔍。`~/status:=state` 把两个节点的私有名都改成 `/robot1/state` 🔍 |
| 用 `--ros-args ... --` 圈出 ROS 参数，可以有多段 | ✅ | `remap_probe myflag --ros-args -r scan:=s1 -- user2 --ros-args -r map:=m1`：两段规则都生效，`myflag`、`user2` 留给用户 🔍 |
| 旧写法 `__ns:=/x`（不带 `--ros-args`） | 🔄 已弃用，但仍然有效 | 生效，同时打印警告 `This syntax is deprecated. Use '--ros-args --remap __ns:=/legacy' instead` 🔍 |
| 运行时的动态重映射 | ⏳ | 没有 |

## 4. 对写代码的影响（规划控制场景）

1. **代码里用相对名，不要用 `/` 开头的绝对名。** 这是重映射和多机器人能工作的前提（见卡片坑 3）。动作服务端也一样：用相对名，部署时一条 `-r` 规则就能改掉整个动作 🔍。
2. **不要指望通配符批量改名。** 设计里的 `**/scan:=\1/scan_filtered` 在 Rolling 会让节点**起不来** 🔍。要批量改，就在 launch 里给每个节点列出规则，或者在代码里把话题名做成参数。💡
3. **一个可执行文件里有多个节点时，规则要加节点前缀。** 实测 launch 的 `Node(name='consumer')` 会变成 `-r __node:=consumer`，作用于进程里的**所有**节点：两个节点都叫 `/robot1/consumer`，rosout 还会警告同名 🔍。组件容器没有这个问题，见 [launch 的设计](launch-design.md)。
4. **同名的话题和服务**（例如地图服务器的 `map` 话题和 `map` 服务）**需要分开改时，用 `rostopic://` / `rosservice://`** 🔍。卡片没提到这个写法。
5. **别单独重映射动作的隐藏话题。** 规则能匹配到 `_action/status`，改了之后这个动作就不完整了 🔍。要改就改动作名。
6. **自己的程序也要解析命令行参数时**，用 `rclcpp::init_and_remove_ros_arguments` 或 `rclcpp::remove_ros_arguments`，拿到的是去掉 `--ros-args` 段后的参数 🔍（见卡片坑 5）。

## 5. 与其他概念的关系

- 卡片：[启动参数与重映射](../cards/configuration/node-arguments.md) · 练习：[launch 与名字解析](../labs/launch-and-names.md)
- [ROS 图怎么映射到 DDS](graph-to-dds-mapping.md)：名字规则、`rt/` 前缀
- [参数的设计](parameter-design.md)：同一套 `--ros-args` 里的 `-p`、`--params-file`
- [launch 的设计](launch-design.md)：launch 把 `remappings=`、`name=`、`namespace=` 翻译成这些命令行参数
- [服务与动作的设计](service-action-design.md)：动作的隐藏话题和服务
- 练习：[配置与启停的设计还成立吗](../labs/config-design-check.md)（第 1 部分）
- 上一级：[总览脑图](../map/overview.md)

## 6. 自测题（先自己答，再看[答案](../answers/remapping-design-answers.md)）

1. 机器人前后各有一个激光雷达，同一个驱动在一个进程里创建了两个节点 `front_lidar` 和 `rear_lidar`（命名空间都是 `/robot1`），两个节点都发布 `scan`。（a）写出把它们分别发布到 `/robot1/front/scan` 和 `/robot1/rear/scan` 的 `--ros-args` 参数。（b）如果按设计文档写成 `-r '**/scan:=\1/scan_raw'` 会发生什么？（c）如果写成 `-r __node:=lidar` 会发生什么？
2. 地图服务器节点 `/map_server` 同时有话题 `map` 和服务 `map`。（a）想让话题改名为 `map_stream`、服务保持不变，怎么写？（b）有人写了 `-r planner:__ns:=/robot2 -r planner:__node:=global_planner`，期望得到 `/robot2/global_planner`，实际得到什么？为什么设计文档要规定"先改节点名、再改命名空间"？
