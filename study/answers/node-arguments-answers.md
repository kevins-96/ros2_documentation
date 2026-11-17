# 答案：启动参数与重映射

> 先在[卡片](../cards/configuration/node-arguments.md)里自己写下答案再来对照。

## 题 1：`odom_node` 的 4 个名字

命令：`--ros-args -r __ns:=/robot2 -r wheel/ticks:=/hw/encoders`

| 代码里 | 结果 | 原因 |
|---|---|---|
| `odom` | `/robot2/odom` | 相对名，放进命名空间 |
| `/tf` | `/tf` | 绝对名，不受命名空间影响 |
| `~/debug` | `/robot2/odom_node/debug` | 私有名 = 命名空间 + 节点名 + 名字 |
| `wheel/ticks` | `/hw/encoders` | 重映射目标写的是绝对名 |

🔍 在 [name_lab](../labs/config_lab/src/name_lab.cpp) 上用同样的规则实测过：`-r sensors/scan:=/front/points` 在 `/robot2` 下得到 `/front/points`。

💡 容易错的两点：
- 以为重映射发生在命名空间之前，把规则左边写成 `/robot2/wheel/ticks`。规则左边按代码里的写法（相对名）写就行，ROS 会按同样的规则解析两边再比较。
- 如果目标写成相对名 `hw/encoders`，结果会是 `/robot2/hw/encoders`。

## 题 2：同时运行前后两个激光驱动

🔍 以下写法都在 name_lab 上实测过。

**写法 A：用命名空间（推荐）**

```bash
ros2 run lidar_pkg lidar_node --ros-args -r __ns:=/front
ros2 run lidar_pkg lidar_node --ros-args -r __ns:=/rear
```

节点全名 `/front/lidar`、`/rear/lidar`；话题 `/front/scan`、`/rear/scan`。私有名和参数服务也自动分开（`/front/lidar/...`）。

**写法 B：改节点名，并把话题重映射到绝对名**

```bash
ros2 run lidar_pkg lidar_node --ros-args -r __node:=front -r scan:=/front/scan
ros2 run lidar_pkg lidar_node --ros-args -r __node:=rear  -r scan:=/rear/scan
```

节点全名 `/front`、`/rear`。

**写法 C：两者都改**

```bash
ros2 run lidar_pkg lidar_node --ros-args -r __node:=front_lidar -r scan:=/front/scan
```

节点 `/front_lidar`。

取舍（💡）：
- 写法 A 最省事，节点里新增的话题也会自动分开，适合"整套东西复制一份"。
- 写法 B、C 每个话题都要单独重映射，漏掉一个就会两个驱动发到同一个话题上。好处是可以精确控制每个名字。
- 两个节点**同名**（都叫 `/lidar`）是必须避免的。📄 文档的 [Feature-Ideas](../../source/The-ROS2-Project/Feature-Ideas.rst) 里"节点名唯一性"仍是待实现的功能，也就是说 ROS 2 不会阻止你启动同名节点。💡 同名后参数服务、`ros2 node info` 等都会混乱；📄 launch 教程也说用不同命名空间是为了避免节点名和话题名冲突。
- 日志 logger 名也跟着变：写法 A 是 `front.lidar`，写法 B 是 `front`。用 `--log-level` 时要写对。
