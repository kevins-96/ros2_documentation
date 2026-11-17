# 答案：组件与组合

> 先在[卡片](../cards/concurrency/component.md)里自己写下答案再来对照。

## 题 1：改成组件后"卡一会儿"、回调不再并行

**现象 1：其他组件启动时卡住。** 改成组件后，原来 `main` 里"等串口最长 10 秒"的代码多半被搬进了构造函数。📄 组件不拥有线程，构造函数是在容器的执行器线程里、处理 `load_node` 服务请求时执行的。🔍 实测在单线程容器里，构造函数睡 6 s，同一容器里其他组件的回调**全部停了 6 秒**。

**现象 2：两个回调组不再并行。** `main` 没了，原来的 `MultiThreadedExecutor` 也跟着没了。默认的 `component_container` 是单线程执行器，回调组设成 `Reentrant` 或分成多个 `MutuallyExclusive` 也只能一个接一个执行（见[回调组](../cards/concurrency/callback-group.md)）。📄 文档专门提醒了这一点。

改法：
1. 构造函数里只声明参数、创建发布者和订阅。等串口放到：
   - 生命周期节点的 `on_configure`（💡 打不开返回 FAILURE，可以重试）。但 `on_configure` 也是在容器的执行器线程里执行的，等 10 秒同样会卡住单线程容器，所以要配合下面两条之一；
   - 后台线程（或带超时的非阻塞打开 + 定时器重试），连上之后再开始工作。这是不卡执行器的根本办法。
2. 容器改成多线程：`component_container --executor-type multi-threaded`（launch 里 `exec="component_container_mt"` 也可以），线程数用 `thread_num` 参数。🔍 实测多线程容器里，即使有组件构造函数阻塞，其他组件也照常运行。

💡 即使用了多线程容器，构造函数阻塞仍然会让 `ros2 component load`（以及 launch 的加载）一直等着，launch 的加载超时后会报错。根本解法是第 1 条。

## 题 2：组件不在容器的命名空间下

原因：📄 "Namespace remappings of the container do not affect loaded components." 容器进程的命名空间只决定容器节点自己的名字（`/robot1/nav_container`），通过 `load_composable_node` 加载的组件不会继承它。🔍 实测 `<node exec="component_container" namespace="robot2"/>` 加 `load_composable_node` 得到的是 `/cloud_filter`，话题是 `/cloud`。

验证：

```bash
ros2 component list        # 看组件的全名
ros2 node list
ros2 topic list
```

改法 A：每个组件写上命名空间。

```xml
<load_composable_node target="/robot1/nav_container">
  <composable_node pkg="my_nav" plugin="my_nav::Controller" name="controller" namespace="robot1"/>
  <composable_node pkg="my_nav" plugin="my_nav::Planner" name="planner" namespace="robot1"/>
</load_composable_node>
```

改法 B：把容器和组件放进同一个 `<group>`，用 `push_ros_namespace`，并把组件写在 `node_container` 里面。🔍 实测这样组件是 `/robot1/fake_lidar`。

```xml
<group>
  <push_ros_namespace namespace="robot1"/>
  <node_container pkg="rclcpp_components" exec="component_container" name="nav_container" namespace="">
    <composable_node pkg="my_nav" plugin="my_nav::Controller" name="controller"/>
    <composable_node pkg="my_nav" plugin="my_nav::Planner" name="planner"/>
  </node_container>
</group>
```

💡 改法 B 更不容易漏，适合多机器人：整组换一个命名空间就行。
