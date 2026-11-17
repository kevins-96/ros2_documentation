# 答案：launch 启动系统

> 先在[卡片](../cards/configuration/launch.md)里自己写下答案再来对照。

## 题 1：写一个 XML launch 文件

💡 参考写法（结构和实测通过的 [bringup_launch.xml](../labs/config_lab/launch/bringup_launch.xml) 相同）：

```xml
<launch>
  <arg name="robot_ns" default="robot1" description="机器人命名空间"/>
  <arg name="sim" default="false" description="是否在仿真中运行"/>

  <group>
    <push_ros_namespace namespace="$(var robot_ns)"/>

    <node pkg="my_nav" exec="controller" name="controller" output="screen">
      <param from="$(find-pkg-share my_nav)/config/controller.yaml"/>
      <param name="use_sim_time" value="$(var sim)"/>
      <remap from="cmd_vel" to="cmd_vel_nav"/>
    </node>
  </group>

  <include file="$(find-pkg-share my_nav)/launch/sim_launch.xml" if="$(var sim)"/>
</launch>
```

检查点：
1. `push_ros_namespace` 必须是 `<group>` 里的**第一个**元素。📄 🔍 放在中间时，它前面的节点不受影响。
2. `use_sim_time` 的 `<param>` 写在 `param from` **之后**，这样命令行的 `sim` 值会覆盖 YAML 里的值。🔍 后写的覆盖前写的。
3. `remap` 的 `from` 写代码里用的名字 `cmd_vel`，不要写 `/robot1/cmd_vel`。结果是 `/robot1/cmd_vel_nav`。
4. `sim_launch.xml` 放在组外还是组内，取决于仿真器要不要在机器人命名空间下。💡 Gazebo 之类的全局仿真器通常放组外。
5. 不要忘了在 `CMakeLists.txt` 里 `install(DIRECTORY launch config ...)`。

常见错误：只写 `<arg name="sim">`，没有通过 `<param>` 传给节点。命令行 `sim:=true` 就只改了 launch 参数，节点的 `use_sim_time` 仍然是 false。

## 题 2：`if use_lidar == 'true'` 为什么永远不成立

`LaunchConfiguration('use_lidar')` 是一个**替换对象**（substitution），只在 launch 执行时才求值。`generate_launch_description()` 运行时还没有求值，它不是字符串。

🔍 实测：`LaunchConfiguration('x') == 'true'` 恒为 `False`，所以节点永远不会被加进去。（反过来写成 `if use_lidar:` 则恒为 `True`，因为对象本身是真值。）两种写法都和命令行传什么无关。

改法：把判断交给 launch 在执行时做。

```python
from launch.conditions import IfCondition

use_lidar = LaunchConfiguration('use_lidar')
return LaunchDescription([
    DeclareLaunchArgument('use_lidar', default_value='true'),
    Node(package='lidar', executable='driver', condition=IfCondition(use_lidar)),
])
```

💡 更复杂的判断用 `PythonExpression`，例如 `IfCondition(PythonExpression(["'", mode, "' == 'real'"]))`。如果确实需要在 Python 里拿到字符串值（比如根据参数决定 include 哪些文件），用 `OpaqueFunction`，它的回调里可以 `LaunchConfiguration('x').perform(context)`。
