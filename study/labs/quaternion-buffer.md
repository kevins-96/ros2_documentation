# 练习：四元数与 tf2 缓冲

> 适用：ROS 2 **Rolling**。对应卡片：[四元数](../cards/coordinates/quaternion.md)、[tf2 与时间](../cards/coordinates/tf2-time.md)
> 代码：[tf_lab/src/tf_offline_lab.cpp](tf_lab/src/tf_offline_lab.cpp)（纯计算，不需要启动其他节点）
> 用时：约 15 分钟

## 0. 编译与运行

编译同[时间参数练习](tf-time-lookup.md)，然后：

```bash
ros2 run tf_lab tf_offline_lab
```

程序分 6 节输出。**先读下面每一节的问题，写下预测，再运行。**

## 1. 先填预测

| 节 | 问题 | 你的预测 |
|---|---|---|
| [1] | $q_{yaw}$ = 绕 Z 转 90°，$q_{roll}$ = 绕 X 转 90°。$q_{yaw} \cdot q_{roll}$ 和 $q_{roll} \cdot q_{yaw}$ 分别把 y 轴 $(0,1,0)$ 转到哪里？ | |
| [2] | `setRPY(0.3, 0.2, 0.1)` 等于 `qz*qy*qx` 还是 `qx*qy*qz`？ | |
| [3] | $q$ 与 $-q$ 旋转 x 轴的结果相同吗？`q.inverse()` 呢？ | |
| [4] | yaw1 = 170°，yaw2 = -170°：直接相减、四元数相对旋转、`atan2` 归一化分别得多少？ | |
| [5] | 把 `(0,0,0,0)`、`(0,0,0,2)`、`(0,0,0.001,1)` 作为 `rotation` 放进 `tf2::BufferCore`，`setTransform` 返回什么？之后能查到吗？ | |
| [6] | 0..20 s 每秒一条 x = t 的变换（默认缓存 10 s）。查 `TimePointZero`、15.5 s、20.2 s、5 s 各得到什么？ | |

<details>
<summary>参考结果（填完预测再展开）</summary>

2026-11-17 在 `ros:rolling-ros-base`（tf2 0.46.5）里实测：

| 节 | 结果 |
|---|---|
| [1] | $q_{yaw} \cdot q_{roll}$ → $(0, 0, 1)$；$q_{roll} \cdot q_{yaw}$ → $(-1, 0, 0)$。$q_a \cdot q_b$ 是**先转 $q_b$ 再转 $q_a$** |
| [2] | `setRPY` = `qz*qy*qx` = (0.1436, 0.1060, 0.0343, 0.9833)；`qx*qy*qz` = (0.1534, 0.0912, 0.0641, 0.9819)。ROS 的 RPY 是固定轴 |
| [3] | $q$ 和 $-q$ 都得 (0.697, 0.717, 0)，yaw 都是 45.8°；`inverse()` 得 (0.697, -0.717, 0)，yaw -45.8° |
| [4] | 直接相减 **-340°**；四元数 20°；`atan2` 20° |
| [5] | `(0,0,0,1)`：true / 能查到；`(0,0,0,0)`：**false / 查不到**；`(0,0,0,2)`：**false / 查不到**；`(0,0,0.001,1)`：true / 能查到。被拒绝时打印 `TF_DENORMALIZED_QUATERNION: Ignoring transform for child_frame_id "base_link" ... because of an invalid quaternion` |
| [6] | `TimePointZero` → x = 20.00；15.5 s → **x = 15.50**（线性插值）；20.2 s → `extrapolation into the future`；5 s → `extrapolation into the past ... earliest data is at time 109`（即 9 s，缓存大约保留最近 10 s） |

解读：
- [5] 是"坐标系不存在"的一个隐蔽原因：消息默认构造的四元数是全 0，发布者如果忘了写 `w = 1`，监听端只会在日志里看到一条 ERROR，之后所有查询都报坐标系不存在。
- [6] 说明 tf2 在两条数据之间做插值，但绝不外推；查询时刻必须落在缓存的时间窗内。

</details>

## 2. 挑战（选做）

1. 在 [6] 里改用 `tf2::BufferCore core(tf2::Duration(std::chrono::seconds(30)));`，预测 5 s 的查询结果。
2. 写一个函数 `double angle_diff(double a, double b)`，返回 $(-\pi, \pi]$ 内的 $a - b$，并用 [4] 的数据和 $a = b$、$a - b = \pi$ 两个边界检验。
