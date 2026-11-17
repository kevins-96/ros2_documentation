# 答案：四元数

> 先在[卡片](../cards/coordinates/quaternion.md)里自己写下答案再来对照。

## 题 1：在 yaw = 30° 的基础上左转 90°

```cpp
tf2::Quaternion q_orig, q_rot;
q_orig.setRPY(0, 0, 30.0 * M_PI / 180);
q_rot.setRPY(0, 0, M_PI / 2);
tf2::Quaternion q_new = q_rot * q_orig;   // 📄 文档写法：新旋转左乘
q_new.normalize();
```

结果 yaw = 120°，$q = (0,\ 0,\ \sin 60°,\ \cos 60°) = (0,\ 0,\ 0.866,\ 0.500)$。（💡 推导：绕 z 轴转 $\psi$ 的四元数是 $(0, 0, \sin\frac{\psi}{2}, \cos\frac{\psi}{2})$。）

说明：这道题里两种乘法顺序结果相同，因为两个旋转都绕 z 轴，可以交换。一旦有 roll 或 pitch，顺序就不能换（🔍 练习第 [1] 节：$q_{yaw} \cdot q_{roll}$ 和 $q_{roll} \cdot q_{yaw}$ 把 y 轴转到完全不同的方向）。左乘 $q_{rot} \cdot q_{orig}$ 表示绕**固定的世界坐标轴**转；右乘 $q_{orig} \cdot q_{rot}$ 表示绕**机器人自身的轴**转（💡 推导，可以用 tf_offline_lab 自己改代码验证）。

## 题 2：航向误差导致绕远路

**情形：** 机器人当前航向 $\psi = 170°$，目标方向在它左后方一点，$\psi_g = -170°$（`atan2` 输出范围是 $(-180°, 180°]$）。`e = psi_g - psi = -340°`，控制器以为要右转 340°，实际左转 20° 就到了。🔍 练习第 [4] 节实测：直接相减 -340°，两种修法都得 20°。

**修法：**

1. 把误差归一化到 $(-\pi, \pi]$：`e = std::atan2(std::sin(e), std::cos(e));`（或 `angles::shortest_angular_distance`，💡 需要依赖 `angles` 包）。
2. 用四元数求相对旋转再取 yaw：`e = tf2::getYaw(q_goal * q_cur.inverse());`。

💡 只要角度会跨越 ±180°（任何转一圈的场景都会），就必须归一化；单元测试里应专门覆盖 170° → -170° 这类用例。
