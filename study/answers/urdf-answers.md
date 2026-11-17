# 答案：URDF 与机器人模型

> 先在[卡片](../cards/coordinates/urdf.md)里自己写下答案再来对照。

## 题 1：加一个俯视 15° 的深度相机

```xml
<link name="camera_link"/>

<joint name="camera_joint" type="fixed">
  <parent link="base_link"/>
  <child link="camera_link"/>
  <origin xyz="0 0 0.3" rpy="0 ${15*pi/180} 0"/>
</joint>
```

- **关节类型 `fixed`**：相机相对底盘不动。📄
- **pitch 取正值：** 💡 推导：绕 y 轴正向转 $\theta$，x 轴变成 $(\cos\theta,\ 0,\ -\sin\theta)$，即朝前朝下。俯视用 $+15°$。可以 `xacro` 展开后用 `robot_state_publisher` + `tf2_echo base_link camera_link` 验证 RPY 是否为 `[0, 15, 0]`。
- `${...}` 只有在 xacro 文件里才能用，纯 URDF 要写数值 `0.2618`。📄
- **出现在 `/tf_static`**：🔍 实测 fixed 关节由 `robot_state_publisher` 发到 `/tf_static`（练习中的 `laser`），可动关节才发到 `/tf`。
- 💡 相机驱动通常还需要一个光学坐标系 `camera_optical_frame`（z 朝前、x 朝右、y 朝下），相对 `camera_link` 的 rpy 为 $(-\frac{\pi}{2}, 0, -\frac{\pi}{2})$。图像和点云的 `frame_id` 应该用光学坐标系。

## 题 2：轮子叠在原点、显示 No transform

车身正常说明 `odom → base_footprint → base_link` 没问题，问题在轮子这段可动关节。📄 某个 link 没有 TF 时，RViz 把它画在原点。排查顺序：

1. **有没有关节状态：** `ros2 topic info /joint_states` 看有没有发布者；`ros2 topic echo --once /joint_states` 看有没有数据。🔍 实测没有 `/joint_states` 时，轮子坐标系根本不存在。
2. **名字对不对：** 对比 `/joint_states` 的 `name` 和 URDF 里的关节名（`check_urdf` 打印树，或 `grep "<joint" robot.urdf`）。名字不一致不会报错，只是没有 TF。💡
3. **TF 本身：** `ros2 run tf2_ros tf2_echo base_link left_wheel`，看是"frame does not exist"（没发布）还是外推错误（时间问题）。`ros2 run tf2_tools view_frames` 看整棵树里有没有轮子。
4. **时间戳：** 如果是外推错误，检查 `/joint_states` 的 `header.stamp` 是否为 0 或墙上时间、是否和 `use_sim_time` 一致。💡
5. **`robot_state_publisher` 是否用了正确的模型：** `ros2 param get /robot_state_publisher robot_description | head` 确认里面有轮子关节；`ros2 node info /robot_state_publisher` 确认它订阅了 `/joint_states`（命名空间、重映射是否对上）。💡

💡 真实底盘上 `/joint_states` 通常由底盘驱动或 `ros2_control` 的 `joint_state_broadcaster` 发布；调模型时可以先用 `joint_state_publisher_gui` 手动拖动关节。
