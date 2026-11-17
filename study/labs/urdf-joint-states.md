# 练习：URDF 关节与 TF

> 适用：ROS 2 **Rolling**。对应卡片：[URDF 与机器人模型](../cards/coordinates/urdf.md)
> 模型：[tf_lab/urdf/mini_bot.urdf.xacro](tf_lab/urdf/mini_bot.urdf.xacro)（差速底盘，左右轮 continuous，激光 fixed）
> 用时：约 20 分钟

## 0. 准备

编译同[时间参数练习](tf-time-lookup.md)。以下命令都在 source 过工作空间的终端里执行：

```bash
D=$(ros2 pkg prefix tf_lab)/share/tf_lab/urdf
xacro $D/mini_bot.urdf.xacro > /tmp/mini.urdf
check_urdf /tmp/mini.urdf
```

## 1. 预测再运行

**第 1 步：只启动 robot_state_publisher**

```bash
ros2 run robot_state_publisher robot_state_publisher --ros-args -p robot_description:="$(cat /tmp/mini.urdf)"
ros2 topic echo --once /tf_static | grep child_frame_id
ros2 run tf2_ros tf2_echo base_link laser
ros2 run tf2_ros tf2_echo base_link left_wheel
```

**第 2 步：发布关节角度**

```bash
ros2 topic pub -r 10 /joint_states sensor_msgs/msg/JointState \
  "{header: {stamp: now}, name: [left_wheel_joint, right_wheel_joint], position: [0.5, 0.0]}"
ros2 run tf2_ros tf2_echo base_link left_wheel
```

**第 3 步：故意写错**

```bash
# 宏名写错一个字母
sed 's/<xacro:wheel prefix="left"/<xacro:wheell prefix="left"/' $D/mini_bot.urdf.xacro > /tmp/typo.xacro
xacro /tmp/typo.xacro > /dev/null; echo "exit=$?"
# 删掉 xacro 命名空间声明
sed 's/ xmlns:xacro="http:\/\/www.ros.org\/wiki\/xacro"//' $D/mini_bot.urdf.xacro > /tmp/nons.xacro
xacro /tmp/nons.xacro > /dev/null; echo "exit=$?"
```

| 问题 | 你的预测 | 实测 |
|---|---|---|
| `check_urdf` 打印的根 link 是谁？有几个子节点？ | | |
| 第 1 步 `/tf_static` 里有哪些 child？ | | |
| 第 1 步 `tf2_echo base_link left_wheel` 能打印吗？ | | |
| 第 2 步左轮相对 `base_link` 的平移和旋转是多少？（用 xacro 里的常量算） | | |
| 宏名写错，xacro 报错吗？ | | |
| 少了命名空间声明，xacro 报错吗？ | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

2026-11-17 在 `ros:rolling-ros-base`（robot_state_publisher 3.6.3，xacro 2.1.1）里实测：

| 问题 | 结果 |
|---|---|
| 根 link | `base_footprint`，1 个子节点 `base_link`，其下 `laser`、`left_wheel`、`right_wheel` |
| `/tf_static` | `base_link`、`laser`（两个 fixed 关节） |
| 第 1 步查左轮 | **不能**，一直报 `"left_wheel" passed to canTransform argument source_frame - frame does not exist` |
| 第 2 步左轮 | Translation `[0.000, 0.165, -0.050]`，RPY `[0, 28.648, 0]`。$0.165 = 0.30/2 + 0.03/2$，$-0.05 = -0.10/2$，28.65° = 0.5 rad 绕 y 轴 |
| 宏名写错 | **报错** `error: unknown macro name: xacro:wheell`，exit = 2（文档说不报错，新版 xacro 已改） |
| 少了命名空间 | 报错 `XML parsing error: unbound prefix`，并提示缺少 `xmlns:xacro` 声明，exit = 2 |

另外，`ros2 topic info -v /robot_description` 显示 Durability 为 TRANSIENT_LOCAL，所以 RViz 晚启动也能拿到模型。

</details>

## 2. 挑战（选做）

1. 给模型加一个装在 `base_link` 上方 0.3 m、俯视 15° 的相机（自测题 1），用 `tf2_echo base_link camera_link` 验证。
2. 把 `left_wheel_joint` 改成 `revolute` 但不写 `<limit>`，`check_urdf` 会怎样？
3. 用 `ros2 run tf2_tools view_frames` 生成 `frames.pdf`，对照 `check_urdf` 的树，找出 `view_frames` 多出来或少了的坐标系，并解释原因。
