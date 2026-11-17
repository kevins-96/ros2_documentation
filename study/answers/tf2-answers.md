# 答案：tf2 坐标变换

> 先在[卡片](../cards/coordinates/tf2.md)里自己写下答案再来对照。

## 题 1：激光障碍物在 odom 里的坐标

**数值（💡 推导，可用 [card_tf2_listener_min](../labs/tf_lab/src/card_tf2_listener_min.cpp) 改参数验证）：**

1. 障碍物在 `laser` 里：$(1, 0)$。
2. 变到 `base_link`：激光装在 $(0.2, 0)$、朝向相同，所以是 $(1.2, 0)$。
3. 变到 `odom`：机器人在 $(2, 1)$，yaw = 90°，旋转矩阵把 $(1.2, 0)$ 转成 $(0, 1.2)$，再平移：

$$
P_{odom} = \begin{pmatrix}2\\1\end{pmatrix} + \begin{pmatrix}\cos 90° & -\sin 90°\\ \sin 90° & \cos 90°\end{pmatrix}\begin{pmatrix}1.2\\0\end{pmatrix} = \begin{pmatrix}2\\2.2\end{pmatrix}
$$

**调用：** `lookupTransform("odom", "laser", t)`，**target 在前，source 在后**，读作"在 odom 里看 laser"。📄 更推荐直接 `buffer->transform(point_in_laser, "odom")`，它从 `header.frame_id` 读 source，你只需给 target。

常见错误：参数顺序写反，得到的是 $_{laser}T_{odom}$，会把 odom 里的点变到 laser，数值完全不同。

## 题 2：tf2_echo 看到的数字和 RViz 不一样

🔍 `tf2_echo X Y` 打印的是 **Y 在 X 里的位姿**（等价于 `lookupTransform(X, Y)`）。`tf2_echo base_link map` 打印的是"地图原点在机器人坐标系里的位置"，是机器人位姿的**逆**，所以和 RViz 里（固定坐标系 `map`）看到的机器人位置不同。

改成：

```bash
ros2 run tf2_ros tf2_echo map base_link
```

这时 Translation 就是机器人在地图里的坐标，RPY 的 yaw 就是航向。

💡 文档把参数写作 `[source_frame] [target_frame]`，按名字理解很容易搞反；记"第一个参数是参考系"。其他可能原因：RViz 的 Fixed Frame 不是 `map`；或者同事看的时刻不同（tf2_echo 持续刷新）。
