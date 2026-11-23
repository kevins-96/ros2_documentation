---
tags: [ros2, launch, 启动, 配置]
distro: rolling
status: batch-3
aliases: [launch, 启动文件, ros2 launch, launch file, 替换, substitution, 事件处理, event handler, PushROSNamespace]
---

# launch 启动系统

> 适用：ROS 2 **Rolling**（2026-11-17 在 `ros:rolling-ros-base` 上实测，launch 3.10.3，launch_ros 0.30.3）
> 来源标记：📄 仓库文档原文 · 🔍 源码或实测核实 · 💡 经验补充（可信，但请自己验证）
> 源文档：[About-Launch.rst](../../../source/Developer-Tools/About-Launch.rst)、[Launch-Main.rst](../../../source/Developer-Tools/Launch/Launch-Main.rst)、[Creating-Launch-Files.rst](../../../source/Developer-Tools/Launch/Creating-Launch-Files.rst)、[Launch-system.rst](../../../source/Developer-Tools/Launch/Launch-system.rst)、[Launch-file-different-formats.rst](../../../source/Developer-Tools/Launch/Launch-file-different-formats.rst)、[Using-Substitutions.rst](../../../source/Developer-Tools/Launch/Using-Substitutions.rst)、[Using-Event-Handlers.rst](../../../source/Developer-Tools/Launch/Using-Event-Handlers.rst)、[Using-ROS2-Launch-For-Large-Projects.rst](../../../source/Developer-Tools/Launch/Using-ROS2-Launch-For-Large-Projects.rst)、[Launching-Multiple-Nodes.rst](../../../source/ROS-Framework/nodes/Working-with-nodes/Launching-Multiple-Nodes/Launching-Multiple-Nodes.rst)
> 📐 设计动机：[launch 的设计](../../design/launch-design.md)：进程与节点分开、SIGINT → SIGTERM → SIGKILL 的升级、事件驱动的启动顺序、XML 前端怎么接到 Python 实现

## 1. 一句话定义

launch 是用一个文件（XML、YAML 或 Python）描述"要启动哪些程序、传什么参数、放在哪个命名空间"，再用一条 `ros2 launch` 命令把整个系统拉起来、监控进程状态并对事件作出反应的工具。📄

## 2. 为什么需要

📄 一个 ROS 2 系统由很多节点组成，分布在多个进程甚至多台机器上，手动逐个启动很快就不可行。💡 一台导航机器人通常要启动驱动、`robot_state_publisher`、定位、规划、控制、可视化十几个节点，每个都有自己的参数文件和重映射。

### 三种格式怎么选 📄

| 格式 | 特点 |
|---|---|
| XML | 简洁，读写方便，大多数场景够用 |
| YAML | 同上 |
| Python | 最灵活，可以用 Python 语言和 launch 的底层 API；代价是更长、更复杂 |

📄 三种格式都能完成常见的启动任务（文档用同一个例子对比三种写法），并且可以互相 include。💡 但不是完全等价：事件处理、`OpaqueFunction` 这类需要写逻辑的功能只有 Python 版完整支持（见下表"事件处理"一行）。文件名推荐以 `_launch.xml` / `_launch.yaml` / `_launch.py` 结尾；Python 文件**必须以 `launch.py` 结尾**才能被 `ros2 launch` 自动补全。

### 核心概念

| 概念 | XML | Python | 说明 |
|---|---|---|---|
| 启动参数 | `<arg name default>` | `DeclareLaunchArgument` | 命令行 `name:=value` 覆盖 📄 |
| 取参数值 | `$(var name)` | `LaunchConfiguration('name')` | 替换（substitution）：**执行时**才求值 📄 |
| 找包路径 | `$(find-pkg-share pkg)` | `FindPackageShare('pkg')` | 📄 |
| Python 表达式 | `$(eval '...')` | `PythonExpression([...])` | 📄 |
| 环境变量 | `$(env NAME)` | `EnvironmentVariable('NAME')` | 📄 |
| 节点 | `<node pkg exec name namespace>` | `Node(...)` | 📄 |
| 节点参数 | `<param name value>` / `<param from="x.yaml">` | `parameters=[yaml, {...}]` | 📄 |
| 重映射 | `<remap from to>` | `remappings=[(a, b)]` | 📄 |
| 包含其他文件 | `<include file>` + `<let>` 传参 | `IncludeLaunchDescription` + `launch_arguments` | 📄 |
| 分组 + 命名空间 | `<group><push_ros_namespace>` | `GroupAction([PushROSNamespace(...)])` | 📄 |
| 条件 | `if="..."` / `unless="..."` | `condition=IfCondition(...)` | 📄 |
| 事件处理 | — | `RegisterEventHandler(OnProcessExit(...))` | 📄 Python 最完整 |

📄 节点参数的两种传法：`args` 直接传给可执行文件（ROS 参数前要自己写 `--ros-args`）；`ros_args`（Python 里叫 `ros_arguments`）专门传 ROS 参数。混用 ROS 和非 ROS 参数时用 `args`。

## 3. 最小示例

[bringup_launch.xml](../../labs/config_lab/launch/bringup_launch.xml)（🔍 实测可运行）：

```xml
<launch>
  <arg name="robot_ns" default="robot1"/>
  <arg name="use_sim_time" default="false"/>
  <arg name="enable_lidar" default="true"/>

  <group>
    <push_ros_namespace namespace="$(var robot_ns)"/>   <!-- 放在组内第一个，才能作用到整个组 -->

    <node pkg="config_lab" exec="card_parameter_min" name="planner" output="screen">
      <param from="$(find-pkg-share config_lab)/config/planner.yaml"/>
      <param name="use_sim_time" value="$(var use_sim_time)"/>   <!-- 后写的覆盖前面的 -->
    </node>

    <node pkg="config_lab" exec="name_lab" name="lidar" args="--spin" output="screen">
      <remap from="scan" to="scan_front"/>
    </node>

    <include file="$(find-pkg-share config_lab)/launch/lidar_launch.xml" if="$(var enable_lidar)"/>
  </group>
</launch>
```

Python 版的事件处理（[bringup_launch.py](../../labs/config_lab/launch/bringup_launch.py)）：**关键节点退出时关闭整个系统**。

```python
RegisterEventHandler(OnProcessExit(
    target_action=planner,
    on_exit=[LogInfo(msg='planner exited, shutting down the whole system'),
             EmitEvent(event=Shutdown(reason='planner exited'))]))
```

📄 其他事件：`OnProcessStart`（进程启动后）、`OnProcessIO`（进程输出）、`OnExecutionComplete`（动作执行完）、`OnShutdown`（launch 被要求关闭）。

安装（C++ 包，📄）：

```cmake
install(DIRECTORY launch config DESTINATION share/${PROJECT_NAME})
```

运行：

```bash
ros2 launch config_lab bringup_launch.xml --show-args          # 列出所有可设置的参数
ros2 launch config_lab bringup_launch.xml robot_ns:=robot2 enable_lidar:=false
ros2 launch ./some_launch.py                                    # 也可以直接给路径
```

### 大型项目的组织方式 📄

- 顶层 launch 文件应该**短**：只包含子系统的 include 和最常改的参数。
- 把相关节点和配置聚成独立的 launch 文件，换机器人、从实机切到仿真只需改顶层文件。
- 参数放 YAML（配置文件，不是 launch 文件），可以从 `ros2 param dump` 导出。
- 用通配符 `/**` 让多个命名空间里的同名节点共用参数。
- 用 `PushROSNamespace` 给整个 include 加命名空间，不用逐个节点写。

### 关闭进程 📄

`ros2 launch` 收到 `SIGINT`（Ctrl+C）或 `SIGTERM` 时，依次给子进程发 `SIGINT`、`SIGTERM`、`SIGKILL`，每次等几秒。终端里按 Ctrl+C 时，终端会同时给 launch 和所有子进程发 `SIGINT`。脚本里只对 launch 进程 `kill -INT` 时，子进程要等几秒后收到 `SIGTERM` 才开始退出；改用 `SIGTERM` 或加 `--noninteractive`。

## 4. 常见坑

🔍 以下结果来自[launch 与名字练习](../../labs/launch-and-names.md)的实测。

1. **`PushROSNamespace` 不在组内第一个。** 📄 必须放在第一个。🔍 组内 `[Node(before), PushROSNamespace('ns_x'), Node(after)]` 的结果是 `/before` 和 `/ns_x/after`，前面的节点不受影响。
2. **`print`/`printf` 在 launch 里看不到。** 🔍 不写 `output="screen"` 时，节点的 `RCLCPP_INFO` 日志（走 stderr）仍显示在屏幕上，但写到 stdout 的 `printf` 输出**不显示**（只进日志文件）。调试时加 `output="screen"`。
3. **以为替换在写文件时就有值。** 📄 `LaunchConfiguration('x')` 是执行时才求值的对象，不能在 Python 里当字符串用。🔍 `LaunchConfiguration('x') == 'true'` 的结果恒为 `False`，而 `bool(LaunchConfiguration('x'))` 恒为 `True`，两者都和命令行传的值无关。条件判断用 `IfCondition`，拼字符串用替换列表。
4. **Python 里把 launch 参数写成字符串字面量。** 🔍 仓库示例 [different_formats_launch.py](../../../source/Developer-Tools/Launch/launch/different_formats_launch.py) 里写的是 `PushROSNamespace('chatter_py_ns')`，用的是字面量 `chatter_py_ns`，而不是同名 launch 参数的值 `chatter/py/ns`。XML 版用的是 `$(var chatter_py_ns)`，所以两个版本的命名空间并不相同。要用参数值，就写 `PushROSNamespace(LaunchConfiguration('chatter_py_ns'))`。（这一条是读源码得出的，没有运行，因为镜像里没有 `demo_nodes_cpp`。）
5. **参数没写说明。** 🔍 `--show-args` 对每个参数都显示 `no description given`，被 include 的文件的参数（例如 `autostart`）也会列出来。💡 用 `description` 属性写清楚，方便别人使用。
6. **launch 参数和节点参数混为一谈。** 💡 `<arg>` 只存在于 launch 文件里，要通过 `<param>` 或 `parameters=` 才能变成节点参数。命令行 `use_sim_time:=true` 只设置 launch 参数，launch 文件里没把它传给节点，就不起作用。
7. **忘了安装 launch 目录。** 📄 C++ 包要 `install(DIRECTORY launch ...)`，Python 包要在 `setup.py` 的 `data_files` 里加上，否则 `ros2 launch` 找不到文件。
8. **一个关键节点挂了，系统其余部分还在跑。** 🔍 默认情况下，launch 不会因为一个子进程退出就关掉其他进程。用 `OnProcessExit` 加 `EmitEvent(Shutdown())`：实测杀掉 planner 后，打印 `planner exited, shutting down the whole system`，`/robot2` 下的节点全部退出。💡 另一种方式是在 Node 上设 `respawn=True` 让它自动重启。

## 5. 与其他概念的关系

- [参数](parameter.md)：launch 里给节点设参数、加载 YAML
- [启动参数与重映射](node-arguments.md)：launch 最终都翻译成 `--ros-args` 命令行，日志里能看到完整命令
- [生命周期节点](lifecycle-node.md)：`lifecycle_node` + `autostart`
- [URDF 与机器人模型](../coordinates/urdf.md)：`Command(['xacro ', path])` 生成 `robot_description`
- [组件与组合](../concurrency/component.md)：`node_container`、`composable_node`、`load_composable_node`
- 练习：[launch 与名字解析](../../labs/launch-and-names.md)
- [launch 的设计](../../design/launch-design.md)（📐 设计动机）：信号升级的实测时间、`name=` 对多节点进程的影响、容器 API
- 上一级：[总览脑图](../../map/overview.md)

## 6. 自测题（先自己答，再看[答案](../../answers/launch-answers.md)）

1. 写一个 XML launch 文件：参数 `robot_ns`（默认 `robot1`）、`sim`（默认 `false`）。在 `robot_ns` 命名空间下启动 `my_nav` 包的 `controller` 节点，加载 `my_nav/config/controller.yaml`，把 `use_sim_time` 设为 `sim` 的值，并把 `cmd_vel` 重映射到 `cmd_vel_nav`。`sim` 为 true 时额外 include `my_nav/launch/sim_launch.xml`。
2. 下面这段 Python 想在 `use_lidar` 为 true 时启动激光节点，为什么不管命令行怎么传都不启动？怎么改？

   ```python
   use_lidar = LaunchConfiguration('use_lidar')
   actions = [DeclareLaunchArgument('use_lidar', default_value='true')]
   if use_lidar == 'true':
       actions.append(Node(package='lidar', executable='driver'))
   return LaunchDescription(actions)
   ```
