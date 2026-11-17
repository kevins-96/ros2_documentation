---
tags: [ros2, tf2, 四元数, 姿态]
distro: rolling
status: batch-2
aliases: [四元数, quaternion, RPY, 欧拉角, yaw, getYaw]
---

# 四元数 Quaternion

> 适用：ROS 2 **Rolling**（2026-11-17 在 `ros:rolling-ros-base` 上编译实测，tf2 0.46.5）
> 来源标记：📄 仓库文档原文 · 🔍 tf2 源码或实测核实 · 💡 经验补充（可信，但请自己验证）
> 源文档：[Quaternion-Fundamentals.rst](../../../source/ROS-Framework/interfaces/Working-with-interfaces/Quaternion-Fundamentals.rst)

## 1. 一句话定义

四元数（quaternion）是用 4 个数 `(x, y, z, w)` 表示三维姿态（orientation）的方法，比旋转矩阵更紧凑；ROS 里**单位四元数**（unit quaternion，模长为 1）表示一个旋转，`w` 放在最后。📄

## 2. 为什么需要

- ROS 的位姿消息（`Pose`、`Transform`、`Odometry`）都用四元数存姿态。📄
- 💡 相比欧拉角（Euler angles）：没有万向节锁（gimbal lock），插值平滑，连乘不需要三角函数。代价是不直观，所以文档建议**用 RPY 思考，再转成四元数**。📄
- 💡 平面导航几乎只关心 yaw：$q = (0,\ 0,\ \sin\frac{\psi}{2},\ \cos\frac{\psi}{2})$，$\psi$ 为航向角。

### 必须记住的几条规则

| 规则 | 公式 | 来源 |
|---|---|---|
| 不旋转 | $(0, 0, 0, 1)$ | 📄 |
| 模长必须为 1 | $x^2+y^2+z^2+w^2 = 1$，否则 ROS 打警告；用 `normalize()` | 📄 |
| 逆 = 共轭（单位四元数） | $q^{-1} = (-x, -y, -z, w)$，**不是**四个分量全取负 | 📄 |
| $q$ 和 $-q$ 是同一个旋转 | 分量不同，旋转相同 | 🔍 实测 |
| 乘法不可交换，顺序有意义 | $q_{new} = q_{rot} \cdot q_{orig}$ | 📄 |
| 相对旋转 | $q_2 = q_r \cdot q_1 \Rightarrow q_r = q_2 \cdot q_1^{-1}$ | 📄 |
| ROS 的 RPY 是固定轴 | `setRPY(r,p,y)` $= R_z(y)\,R_y(p)\,R_x(r)$ | 📄 定义，🔍 数值核实 |

📄 固定轴（fixed / static frame）RPY：三次旋转都绕**原始不动**的坐标轴；相对的是"动轴"（relative frame），每次绕上一次转过之后的轴。🔍 [练习](../../labs/quaternion-buffer.md)里 `setRPY(0.3, 0.2, 0.1)` 与 `qz*qy*qx` 逐位相同，与 `qx*qy*qz` 不同。

💡 乘法顺序怎么读：$q_a \cdot q_b$ 作用到向量上时，**先转 $q_b$，再转 $q_a$**（和矩阵 $R_a R_b v$ 一样从右往左）。

## 3. 最小 C++ 代码

[card_quaternion_min.cpp](../../labs/tf_lab/src/card_quaternion_min.cpp)：

```cpp
#include "tf2/LinearMath/Matrix3x3.hpp"
#include "tf2/LinearMath/Quaternion.hpp"
#include "tf2/utils.hpp"                              // tf2::getYaw
#include "tf2_geometry_msgs/tf2_geometry_msgs.hpp"    // tf2::toMsg / fromMsg

tf2::Quaternion q;
q.setRPY(0.0, 0.0, M_PI / 2);                 // 输出 (0, 0, 0.707, 0.707)

geometry_msgs::msg::Quaternion msg = tf2::toMsg(q);   // tf2 -> 消息
tf2::Quaternion back;
tf2::fromMsg(msg, back);                              // 消息 -> tf2（也可 tf2::convert）

double yaw = tf2::getYaw(msg);                // 平面导航最常用
double r, p, y;
tf2::Matrix3x3(back).getRPY(r, p, y);         // 完整 RPY

tf2::Quaternion q1, q2;
q1.setRPY(0, 0, 0.3);
q2.setRPY(0, 0, 1.0);
tf2::Quaternion q_r = q2 * q1.inverse();      // 相对旋转，yaw = 0.7
q_r.normalize();                              // 连乘后归一化
```

📄 ROS 2 有两种四元数类型：`tf2::Quaternion`（能做运算）和 `geometry_msgs::msg::Quaternion`（只是消息数据），用 `tf2_geometry_msgs` 互转。

## 4. 常见坑

1. **把 Eigen 的顺序当成 ROS 的顺序。** 📄 ROS 是 `(x, y, z, w)`，Eigen 的 `Quaterniond(w, x, y, z)` 构造函数是 `w` 在前。💡 跨库转换时显式按名字赋值，不要按下标。
2. **四元数全 0 或没归一化。** 🔍 放进 `tf2::BufferCore` 实测：`(0,0,0,0)` 和 `(0,0,0,2)` 都让 `setTransform` 返回 `false`，打印 `TF_DENORMALIZED_QUATERNION: Ignoring transform ...`，之后查询不到这个坐标系；`(0,0,0.001,1)` 这种小误差被接受。💡 消息默认构造就是全 0，忘了写 `w = 1` 是最常见的"坐标系不存在"原因之一。
3. **航向差直接相减。** 🔍 yaw1 = 170°、yaw2 = -170°，直接相减得 -340°，实际只需转 +20°。用 `getYaw(q2 * q1.inverse())` 或 `atan2(sin d, cos d)` 都得 20°。💡 角度误差进 PID 前必须归一化到 $(-\pi, \pi]$，否则机器人会绕一大圈。
4. **以为"取逆"是四个分量全取负。** 📄 那样得到的是 $-q$，和 $q$ 是同一个旋转。🔍 实测 $q$ 和 $-q$ 旋转 x 轴结果都是 (0.697, 0.717)，`inverse()` 才是 (0.697, -0.717)。
5. **乘法顺序写反。** 🔍 绕 Z 转 90° 记作 $q_{yaw}$，绕 X 转 90° 记作 $q_{roll}$：$q_{yaw} \cdot q_{roll}$ 把 y 轴转到 (0, 0, 1)，$q_{roll} \cdot q_{yaw}$ 把 y 轴转到 (-1, 0, 0)。
6. **比较两个四元数是否相等时逐分量比较。** 💡 $q$ 和 $-q$ 相同，应比较 $|q_1 \cdot q_2|$（点积绝对值）接近 1，或比较相对旋转的角度。

## 5. 与其他概念的关系

- [tf2 坐标变换](tf2.md)：`transform.rotation` 就是四元数
- [URDF 与机器人模型](urdf.md)：URDF 里写的是 `rpy`，由工具转成四元数
- 练习：[四元数与 tf2 缓冲](../../labs/quaternion-buffer.md)
- 上一级：[总览脑图](../../map/overview.md)

## 6. 自测题（先自己答，再看[答案](../../answers/quaternion-answers.md)）

1. 底盘当前航向 yaw = 30°，想原地再左转 90°。写出新姿态的四元数计算（用 `tf2::Quaternion`），并写出最终 `(x, y, z, w)` 的数值（保留 3 位小数）。
2. 你的路径跟踪控制器用 `atan2` 算出目标航向 $\psi_g$，用 `getYaw` 取当前航向 $\psi$，误差写成 `e = psi_g - psi`。描述一种会让机器人"绕远路转一圈"的情形，并给出两种修法。
