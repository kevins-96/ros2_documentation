# 练习：配置与启停的设计在 Rolling 里还成立吗？

> 适用：ROS 2 **Rolling**（默认 RMW `rmw_fastrtps_cpp`）。对应设计笔记：[名字与重映射的设计](../design/remapping-design.md)、[参数的设计](../design/parameter-design.md)、[生命周期的设计](../design/lifecycle-design.md)、[时钟与时间的设计](../design/time-design.md)、[launch 的设计](../design/launch-design.md)
> 代码：[design_lab/](design_lab/)（本批新增 `remap_probe`、`param_design_probe`、`lifecycle_design_probe`、`time_design_probe`、组件 `design_lab::NameProbe`、脚本 `signal_logger.sh` 和 `launch/` 下的 4 个 launch 文件）
> 用时：约 50 分钟，可以分几次做

做法和第 8 批的[练习](design-claims-check.md)一样：先填预测，再运行，最后展开参考结果。**预测错了的地方，就是你对设计理解有偏差的地方。**

## 0. 编译

```bash
ln -s <你的仓库路径>/study/labs/* ~/study_ws/src/
cd ~/study_ws && source /opt/ros/rolling/setup.bash
colcon build --packages-up-to design_lab && source install/setup.bash
```

或者用官方镜像：`docker run -it --rm -v <你的仓库路径>/study/labs:/ws/src/labs ros:rolling-ros-base`，在容器里执行 `cd /ws && colcon build --packages-up-to design_lab`。

## 1. 重映射规则

`remap_probe` 在一个进程里建两个节点 `/robot1/planner`、`/robot1/controller`。它打印代码里写的名字（`scan`、`/scan/head/scan`、`map`、`~/status`、`/diagnostics`）被解析成什么话题名和服务名，再在 planner 上建一个同名的话题和服务 `map`，以及一个动作服务端 `move_to`。

```bash
ros2 run design_lab remap_probe                                   # 基准
ros2 run design_lab remap_probe --ros-args -r planner:scan:=scan_filtered
ros2 run design_lab remap_probe --ros-args -r '**/scan:=\1/scan_filtered'
ros2 run design_lab remap_probe --ros-args -r 'rostopic://map:=map_stream'
ros2 run design_lab remap_probe --ros-args -r planner:__ns:=/a -r planner:__node:=p2
ros2 run design_lab remap_probe --ros-args -r planner:__node:=p2 -r p2:__ns:=/a
ros2 run design_lab remap_probe --ros-args -r planner:__ns:=/foo -r __ns:=/bar
ROS_NAMESPACE=/envns ros2 run design_lab remap_probe
ros2 run design_lab remap_probe --ros-args -r move_to:=move_to_checked
ros2 run design_lab remap_probe --ros-args -r /robot1/move_to/_action/status:=/x
ros2 run design_lab remap_probe myflag --ros-args -r scan:=s1 -- user2 --ros-args -r map:=m1
```

| 问题 | 你的预测 | 实测 |
|---|---|---|
| `planner:scan:=scan_filtered` 会不会影响 controller？ | | |
| 设计文档里的通配符 `**/scan:=\1/scan_filtered` 能用吗？ | | |
| `rostopic://map:=map_stream` 之后，planner 的 `map` 话题和 `map` 服务各叫什么？ | | |
| `planner:__ns:=/a` 和 `planner:__node:=p2` 一起给，节点最后的全名是什么？ | | |
| `planner:__ns:=/foo` 在前、`__ns:=/bar` 在后，两个节点各在哪个命名空间？ | | |
| `ROS_NAMESPACE` 环境变量有效吗？ | | |
| `move_to:=move_to_checked` 一条规则能改掉动作的几个隐藏名字？ | | |
| 单独重映射 `/robot1/move_to/_action/status` 能成功吗？ | | |
| 最后一条命令：两段 `--ros-args` 都生效吗？程序拿到哪些用户参数？ | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

2026-11-23 在 `ros:rolling-ros-base`（rclcpp 33.2.0、rcl 10.6.0）里实测，只列出和基准不同的行：

```text
基准：planner 和 controller 的 scan -> /robot1/scan，map -> /robot1/map，
      ~/status -> /robot1/<节点名>/status；动作名字为 /robot1/move_to/_action/{feedback,status,send_goal,cancel_goal,get_result}

-r planner:scan:=scan_filtered
  planner    scan -> /robot1/scan_filtered      controller 不变

-r '**/scan:=\1/scan_filtered'
  init 失败: ... Couldn't parse remap rule ... Error: Wildcard '**' is not implemented
  （'/scan/*/scan:=...' 同样报 Wildcard '*' is not implemented）

-r 'rostopic://map:=map_stream'
  map  话题 -> /robot1/map_stream   服务 -> /robot1/map
  （rosservice://map:=map_srv 只改服务；替换侧写 URL 会解析失败）

-r planner:__ns:=/a -r planner:__node:=p2
  [p2] 节点全名 = /robot1/p2          ← 命名空间规则没生效
-r planner:__node:=p2 -r p2:__ns:=/a
  [p2] 节点全名 = /a/p2

-r planner:__ns:=/foo -r __ns:=/bar
  planner -> /foo/planner    controller -> /bar/controller

ROS_NAMESPACE=/envns
  （和基准完全一样，环境变量被忽略）

-r move_to:=move_to_checked
  5 个名字全部变成 /robot1/move_to_checked/_action/...

-r /robot1/move_to/_action/status:=/x
  动作话题 /x      ← status 被单独改走，其余不变

myflag --ros-args -r scan:=s1 -- user2 --ros-args -r map:=m1
  留给用户的参数: [myflag] [user2]
  scan -> /robot1/s1    map -> /robot1/m1

旧写法 remap_probe __ns:=/legacy
  [WARN] [rcl]: Found remap rule '__ns:=/legacy'. This syntax is deprecated. ...
  节点照样移到 /legacy
```

另外：`-r __node:=p2`（不带前缀）把两个节点都改成 `/robot1/p2`，并打印 rosout 同名警告。节点前缀不能带命名空间，`-r /robot1/planner:scan:=x` 会解析失败。

</details>

## 2. 参数

### 2a. 参数的 API

```bash
ros2 run design_lab param_design_probe
```

planner 声明了 `max_speed`（范围 [0, 2]）、`max_accel`、`frame_id`、`mask`（`bool[]`）、`blob`（`byte[]`），注册了校验回调"`max_accel` 必须小于 `max_speed`"。watcher 订阅 `/parameter_events`，并且用 `ParameterEventHandler` 只盯 `max_speed`。

| 问题 | 你的预测 | 实测 |
|---|---|---|
| 设置没声明过的 `max_jerk` 会怎样？ | | |
| `max_speed=3.0`（超出范围）谁来拒绝：框架还是你的回调？ | | |
| 原子设置 `{max_speed=1.5, max_accel=2.0}` 后两个值各是多少？换成非原子的 `set_parameters` 呢？ | | |
| 被拒绝的修改会发参数事件吗？声明新参数会发吗？ | | |
| 参数事件话题叫什么？每个节点一个吗？ | | |

<details>
<summary>参考结果</summary>

```text
A. 参数服务（节点自带）:
  /robot1/planner/describe_parameters
  /robot1/planner/get_parameter_types
  /robot1/planner/get_parameters
  /robot1/planner/get_type_description
  /robot1/planner/list_parameters
  /robot1/planner/set_parameters
  /robot1/planner/set_parameters_atomically
B. 设置未声明的参数:
  set max_jerk=1.0 抛异常: parameter 'max_jerk' cannot be set because it was not declared
C. 范围检查（描述里的 floating_point_range）:
  set max_speed=3.0（超出 [0,2]）          -> 拒绝 Parameter {max_speed} doesn't comply with floating point range.
D. 原子批量设置（max_speed=1.5 合法，max_accel=2.0 违反回调）:
  set_parameters_atomically                    -> 拒绝 max_accel 必须小于 max_speed
  结果：max_speed=1.00 max_accel=0.50
E. 非原子批量设置（同样两个值，逐个设置）:
  set_parameters                               -> [成功] [拒绝: max_accel 必须小于 max_speed]
    [单参数回调] max_speed -> 1.50
    [事件] node=/robot1/planner new=0 changed=1 deleted=0: max_speed
  结果：max_speed=1.50 max_accel=0.50
F. 被拒绝的修改会不会发事件？（上面每次拒绝后都没有 [事件] 行就说明不会）共收到 1 条事件
G. 描述参数（远端工具能看到的约束）:
  max_speed type=3 range=[0.0, 2.0] desc="最大线速度 m/s"
  blob type=5 desc=""
  mask type=6 desc=""
H. 嵌套参数名用 '.' 分隔:
  controller.kp
    [事件] node=/robot1/planner new=1 changed=0 deleted=0: +controller.kp
```

watcher 订阅的是全局话题 `/parameter_events`，用消息里的 `node` 字段区分是哪个节点发的。

</details>

### 2b. 命令行原子设置

```bash
ros2 run design_lab param_design_probe --serve      # 终端 1：只声明参数并 spin
ros2 param set /robot1/planner max_speed 0.3 max_accel 0.5            # 终端 2
ros2 param set /robot1/planner max_speed 1.0
ros2 param set /robot1/planner max_accel 0.2 max_speed 0.3 --atomic
ros2 param get /robot1/planner max_speed && ros2 param get /robot1/planner max_accel
```

<details>
<summary>参考结果</summary>

```text
max_accel: Set parameter successful
max_speed: Setting parameter failed: max_accel 必须小于 max_speed     ← 不加 --atomic：只改了一半
...
Set parameters atomically successful
Double value is: 0.3
Double value is: 0.2
```

</details>

### 2c. `-p` 和 `--params-file` 落到哪个节点

```bash
cat > /tmp/p1.yaml <<'EOF'
/**:
  ros__parameters: {tag: all}
/robot1/*:
  ros__parameters: {in_robot1: true}
/**/planner:
  ros__parameters: {only_planner: 1}
planner:
  ros__parameters: {bare_name: 1}
EOF
ros2 run design_lab remap_probe --ros-args --params-file /tmp/p1.yaml
ros2 run design_lab remap_probe --ros-args -p planner:max_speed:=1.5
ros2 run design_lab remap_probe --ros-args -r __ns:=/ -p planner:max_speed:=1.5
```

再自己写两个文件，把 `/**: {order: first}` 和 `/robot1/planner: {order: second}` 按不同顺序排列，分别和 `-p order:=cli` 组合（`-p` 放在 `--params-file` 前面或后面），看 planner 和 controller 各拿到什么。

| 问题 | 你的预测 | 实测 |
|---|---|---|
| `bare_name`（键 `planner:`）设到 `/robot1/planner` 上了吗？ | | |
| `-p planner:max_speed:=1.5` 对 `/robot1/planner` 有效吗？对 `/planner` 呢？ | | |
| YAML 里 `/**` 和 `/robot1/planner` 都设了 `order`，谁赢？和顺序有关吗？ | | |
| `-p order:=cli` 能盖过 YAML 里 `/robot1/planner` 的值吗？ | | |

<details>
<summary>参考结果</summary>

```text
--params-file /tmp/p1.yaml
  planner:    in_robot1 = true, only_planner = 1, tag = all      （没有 bare_name）
  controller: in_robot1 = true, tag = all

-p planner:max_speed:=1.5                    → 两个节点都没有 max_speed（静默无效）
-p /robot1/planner:max_speed:=1.5            → 解析报错 Expected lexeme type 22, got 5
-r __ns:=/ -p planner:max_speed:=1.5         → /planner 的 max_speed = 1.5

YAML 键写 /robot1/plan*                       → 解析报错 node name must not contain characters other than alphanumerics or '_'

YAML 先 /**: first 后 /robot1/planner: second → planner = second, controller = first
YAML 先 /robot1/planner: second 后 /**: first → planner = first,  controller = first
-p order:=cli --params-file（先 /** 后全名）  → planner = second, controller = first
--params-file（先 /** 后全名） -p order:=cli  → planner = second, controller = cli
```

规律：键按**在合并后的参数表里第一次出现的顺序**依次套用，后套用的覆盖先套用的；`-p` 属于 `/**` 这个键。

</details>

## 3. 生命周期

```bash
ros2 run design_lab lifecycle_design_probe
```

| 问题 | 你的预测 | 实测 |
|---|---|---|
| `on_activate` 返回 FAILURE 后节点在什么状态？设计文档说进 ErrorProcessing | | |
| 返回 ERROR、抛异常、抛异常且 `on_error` 返回 FAILURE，各到什么状态？ | | |
| 管理接口有几个服务？是不是每个转换一个？ | | |
| `transition_event` 的 durability 是什么？设计文档说要锁存 | | |
| Finalized 之后再 configure，带返回码参数的 `configure(rc)` 会把 `rc` 写成什么？ | | |

<details>
<summary>参考结果</summary>

```text
A. 回调结果把节点带到哪里:
on_activate 返回 SUCCESS                     activate 后状态 = active       (返回码 97)
on_activate 返回 FAILURE                     activate 后状态 = inactive     (返回码 98)
on_activate 返回 ERROR，on_error 成功         activate 后状态 = unconfigured (返回码 99)
on_activate 抛异常，on_error 成功             activate 后状态 = unconfigured (返回码 99)
on_activate 抛异常，on_error 返回 FAILURE     activate 后状态 = finalized    (返回码 99)
  （日志：Caught exception in callback for transition 13 / Original error: lidar not responding
          on_error (from inactive, mode=ok)）
B. 管理接口（服务、话题）:
  服务 /lidar_driver/change_state               lifecycle_msgs/srv/ChangeState
  服务 /lidar_driver/get_available_states       lifecycle_msgs/srv/GetAvailableStates
  服务 /lidar_driver/get_available_transitions  lifecycle_msgs/srv/GetAvailableTransitions
  服务 /lidar_driver/get_state                  lifecycle_msgs/srv/GetState
  服务 /lidar_driver/get_transition_graph       lifecycle_msgs/srv/GetAvailableTransitions
  话题 /lidar_driver/transition_event  lifecycle_msgs/msg/TransitionEvent  depth=10 reliability=reliable durability=volatile
C. 从各个主状态能不能直接 shutdown:
  从 unconfigured shutdown -> finalized
  再 configure -> finalized (返回码 255；CallbackReturn 是 uint8，255 表示没被改写)
  从 inactive     shutdown -> finalized
  从 active       shutdown -> finalized
  （日志：No transition matching 1 found for current state finalized）
```

</details>

## 4. 时钟与时间

```bash
ros2 run design_lab time_design_probe
```

| 问题 | 你的预测 | 实测 |
|---|---|---|
| `use_sim_time=true`、还没收到 `/clock` 时 `now()` 是多少？ | | |
| `/clock` 从 100 s 跳回 40 s：跳变前回调里 `now()` 是多少？跳变后回调报的 delta 是多少？ | | |
| `/clock` 以 10 倍速推进 1 s（墙钟），0.5 s 周期的 ROS 定时器和墙钟定时器各触发几次？暂停 2 s 呢？ | | |
| `steady` 时间和 `ros` 时间比较，是编译失败还是运行时失败？`system` 和 `ros` 呢？ | | |
| `rclcpp::Rate(10)` 默认用什么时钟？ | | |
| rclcpp 订阅 `/clock` 用什么 QoS？ | | |

<details>
<summary>参考结果</summary>

```text
A. 收到 /clock 之前: now() = 0.000 s, clock 类型 = 1 (RCL_ROS_TIME=1), ros_time_is_active = 1
B. /clock 发布 100 s，然后往回跳到 40 s:
    [跳变前回调] 此时 now() = 0.000
    [跳变后回调] 时间跳变, delta = 100.000 s, 此时 now() = 100.000
  now() = 100.000
    [跳变前回调] 此时 now() = 100.000
    [跳变后回调] 时间跳变, delta = -60.000 s, 此时 now() = 40.000
  now() = 40.000
C. /clock 以 10 倍速推进 1 s 墙钟（仿真走 10 s），再暂停 2 s 墙钟:
  推进阶段: ROS 定时器 20 次, 墙钟定时器 2 次
  暂停阶段: ROS 定时器 0 次, 墙钟定时器 4 次
D. 不同时钟类型的时间能不能比较:
  steady < ros             编译通过，运行时抛异常: can't compare times with different time sources
  system < ros             编译通过，运行时抛异常: can't compare times with different time sources
  ros < ros                编译通过，运行结果 0
E. rclcpp::Rate 默认时钟:
  Rate(10) 的时钟类型 = 2 (SYSTEM=2, ROS=1)
  Rate(10, node clock) 的时钟类型 = 1
F. /clock 的 QoS:
  订阅者 planner: depth=1 reliability=best_effort
G. 关掉 use_sim_time:
    [跳变前回调] 此时 now() = 50.000
    [跳变后回调] ROS 时间被关闭, delta = 0.000 s, 此时 now() = 1795438833.629
  now() = 1795438834 s（墙钟）
```

把程序里的 `th.min_forward.nanoseconds` 改成 1 再跑一次：推进阶段的每一次 `/clock` 更新都会触发一对跳变回调。

</details>

## 5. launch

### 5a. 信号升级

```bash
ros2 launch design_lab signal_escalation.launch.py log:=/tmp/esc.log &
sleep 3; kill -INT %1; wait; cat /tmp/esc.log       # 再把 -INT 换成 -TERM 试一次
```

`signal_logger.sh` 忽略 SIGINT 和 SIGTERM，并记录收到它们的时刻（从脚本启动算起）。

| 问题 | 你的预测 | 实测 |
|---|---|---|
| 对 launch 发 SIGINT：子进程依次收到什么？间隔多久？launch 多久退出？ | | |
| 对 launch 发 SIGTERM：设计文档说立刻 SIGKILL 子进程，实际呢？ | | |
| launch 文件没声明 `sigterm_timeout`，命令行写 `sigterm_timeout:=1` 有用吗？ | | |

<details>
<summary>参考结果</summary>

```text
===== 对 launch 发 SIGINT：launch 退出码 0，耗时 10.1 s
SIGINT 2.53 s
SIGTERM 7.55 s
[ERROR] [stubborn-1]: process[stubborn-1] failed to terminate '5' seconds after receiving 'SIGINT', escalating to 'SIGTERM'
[ERROR] [stubborn-1]: process[stubborn-1] failed to terminate '10.0' seconds after receiving 'SIGTERM', escalating to 'SIGKILL'
[ERROR] [stubborn-1]: process has died [pid 2953, exit code -9, ...]

===== 对 launch 发 SIGTERM：launch 退出码 0，耗时 10.1 s
SIGINT 2.68 s          ← launch 收到 SIGTERM，转发给子进程的仍是 SIGINT
SIGTERM 7.70 s
（之后同样 SIGKILL）

没声明参数的 launch 文件 + 命令行 sigterm_timeout:=1 sigkill_timeout:=1
SIGINT 1.69 s
SIGTERM 2.73 s         ← 1 s 后就升级了
```

日志里的 `'10.0' seconds after receiving 'SIGTERM'` 其实是 5 s + 5 s 的累计，SIGTERM 之后只等了 5 s。

</details>

### 5b. 用生命周期事件编排启动顺序

```bash
ros2 launch design_lab lifecycle_chain.launch.py      # Ctrl-C 结束
```

| 问题 | 你的预测 | 实测 |
|---|---|---|
| on_configure、on_activate、consumer 启动，日志里的先后顺序？ | | |
| launch 里写了 `Node(name='consumer')`，而 `remap_probe` 一个进程里有两个节点，它们各叫什么？ | | |

<details>
<summary>参考结果</summary>

```text
[lidar_driver]: on_configure
[launch.user]: driver 已 inactive，请求 activate
[lidar_driver]: on_activate (mode=ok)
[launch.user]: driver 已 active，启动 consumer
[INFO] [remap_probe-2]: process started with pid [...]
[remap_probe-2] [WARN] [rcl.logging_rosout]: Publisher already registered for node name: 'consumer'. ...
[remap_probe-2] [consumer] 节点全名 = /robot1/consumer
[remap_probe-2] [consumer] 节点全名 = /robot1/consumer      ← 两个节点都被改名了
...
（Ctrl-C 后）[rclcpp_lifecycle]: LifecycleNode is not shut down: Node still in state(3) in destructor
```

</details>

### 5c. XML 前端和 Python 写法对比

```bash
ros2 launch design_lab frontend_demo.launch.xml
ros2 launch design_lab frontend_demo.launch.py
ros2 launch design_lab frontend_demo.launch.xml use_lidar:=false
ros2 launch design_lab frontend_demo.launch.xml --show-args
```

<details>
<summary>参考结果</summary>

两种写法打印的参数和重映射完全一样：

```text
[planner] 节点全名 = /robot1/planner
  参数 frame_ids = [map, odom, base_link]
  参数 limits.accel = 1.000000
  参数 max_speed = 1.000000
  scan             话题 -> /robot1/scan_filtered
```

`use_lidar:=false` 时节点不启动。试着在 XML 注释里写上 `--show-args` 再运行：整个文件解析失败，报 `not well-formed (invalid token): line 2, column 58`。XML 注释里不能出现连续的两个减号。

</details>

### 5d. 组件容器

```bash
ros2 run rclcpp_components component_container --ros-args -r __node:=cont -r scan:=container_scan -p tag:=from_container   # 终端 1
ros2 service list --include-hidden-services | grep _container                       # 终端 2
ros2 component load /cont design_lab design_lab::NameProbe
ros2 component load /cont design_lab design_lab::NameProbe                           # 同名再加载一次
ros2 component load /cont design_lab design_lab::NameProbe -n probe2 -r scan:=own_scan -p tag:=own
ros2 component list
```

| 问题 | 你的预测 | 实测 |
|---|---|---|
| 容器的 `-r scan:=container_scan`、`-p tag:=from_container` 会传给加载的节点吗？ | | |
| 同名节点第二次加载会被拒绝吗？设计文档要求拒绝 | | |

<details>
<summary>参考结果</summary>

```text
/cont/_container/list_nodes
/cont/_container/load_node
/cont/_container/unload_node
Loaded component 1 into '/cont' container node as '/name_probe'
Loaded component 2 into '/cont' container node as '/name_probe'      ← 没被拒绝
Loaded component 3 into '/cont' container node as '/probe2'
/cont
  1  /name_probe
  2  /name_probe
  3  /probe2
容器日志:
[name_probe]: 全名=/name_probe scan->/scan tag=<未设置>             ← 容器的参数没传过来
[WARN] [rcl.logging_rosout]: Publisher already registered for node name: 'name_probe'. ...
[probe2]: 全名=/probe2 scan->/own_scan tag=own
```

</details>

## 做完之后

回到各篇设计笔记的对照表，把你预测错的那几行再读一遍；然后合上页面，做笔记末尾的自测题。
