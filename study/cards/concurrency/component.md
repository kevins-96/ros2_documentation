---
tags: [ros2, rclcpp, 组件, composition, 进程布局]
distro: rolling
status: batch-4
aliases: [组件, component, 组合, composition, 组件容器, component container, ComposableNode, rclcpp_components, nodelet]
---

# 组件与组合 Component & Composition

> 适用：ROS 2 **Rolling**（2026-11-17 在 `ros:rolling-ros-base` 上编译实测，rclcpp / rclcpp_components 33.2.0，launch_ros 0.30.3）
> 来源标记：📄 仓库文档原文 · 🔍 源码或实测核实 · 💡 经验补充（可信，但请自己验证）
> 源文档：[About-Composition.rst](../../../source/ROS-Framework/nodes/About-Composition.rst)、[Composition.rst](../../../source/ROS-Framework/nodes/Working-with-nodes/Composition.rst)、[Writing-a-Composable-Node.rst](../../../source/ROS-Framework/nodes/Working-with-nodes/Writing-a-Composable-Node.rst)、[Launching-composable-nodes.rst](../../../source/Developer-Tools/Launch/Launching-composable-nodes.rst)

## 1. 一句话定义

组件（component）是编译成**共享库**、没有 `main` 函数的节点类；部署时再决定把哪些组件装进同一个**组件容器**（component container）进程，还是各自单独运行。📄

## 2. 为什么需要

📄 把"进程怎么划分"变成部署时的决定：

| 布局 | 好处 | 代价 |
|---|---|---|
| 每个节点一个进程 | 进程/故障隔离，单独调试方便 | 进程间通信要序列化、走中间件 |
| 多个节点一个进程 | 开销小，可以用[进程内通信](intra-process.md)避免复制 | 一个组件崩溃，整个容器一起崩 |

🔍 实测（[perf_lab](../../labs/perf_lab/)，4 MB 点云、10 Hz、SensorDataQoS）：

| 布局 | 2 秒内收到 | 平均延迟 |
|---|---|---|
| 两个进程 | **0~1 帧**（其余全丢） | — |
| 同一容器，进程内通信关 | 20 帧 | 约 3.1 ms |
| 同一容器，进程内通信开 | 20 帧 | 约 0.7 ms，地址相同（零拷贝） |

💡 两进程丢帧的原因很可能是大消息在 best effort 下分片丢失（100 KB 时两进程也能收齐 20 帧），这属于 DDS 调优（第 7 批）。但它说明了一件事：激光、相机、点云这类大数据的处理链，放进同一个容器往往是最直接的解法。Nav2 的 `use_composition` 就是这个思路。

### 容器类型 📄

| 启动方式 | 执行器 |
|---|---|
| `component_container` | 一个 `SingleThreadedExecutor` |
| `component_container --executor-type multi-threaded` | 一个 `MultiThreadedExecutor`，线程数用参数 `thread_num` |
| `component_container --executor-type events-cbg` | 一个 `EventsCBGExecutor` |
| 以上加 `--isolated` | 每个组件一个独立执行器 |

🔍 Rolling 镜像里还装有 `component_container_mt`、`component_container_isolated`、`component_container_event` 三个可执行文件。

## 3. 最小 C++ 代码

[fake_lidar_component.cpp](../../labs/perf_lab/src/fake_lidar_component.cpp)（节选）：

```cpp
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_components/register_node_macro.hpp"
#include "sensor_msgs/msg/point_cloud2.hpp"

namespace perf_lab
{
class FakeLidar : public rclcpp::Node
{
public:
  // 📄 构造函数必须接收 NodeOptions：容器通过它传入名字、参数、extra_arguments
  explicit FakeLidar(const rclcpp::NodeOptions & options)
  : Node("fake_lidar", options)
  {
    pub_ = create_publisher<sensor_msgs::msg::PointCloud2>("cloud", rclcpp::SensorDataQoS());
    timer_ = create_wall_timer(std::chrono::milliseconds(100), [this]() {
        auto msg = std::make_unique<sensor_msgs::msg::PointCloud2>();
        msg->header.stamp = now();
        msg->data.resize(4000000);
        pub_->publish(std::move(msg));     // 交出所有权，进程内可以零拷贝
      });
  }
private:
  rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};
}  // namespace perf_lab

RCLCPP_COMPONENTS_REGISTER_NODE(perf_lab::FakeLidar)   // 📄 代替 main，让组件可被发现
```

CMake（📄）：

```cmake
find_package(rclcpp_components REQUIRED)
add_library(lidar_components SHARED src/fake_lidar_component.cpp src/cloud_filter_component.cpp)
target_link_libraries(lidar_components rclcpp::rclcpp rclcpp_components::component ${sensor_msgs_TARGETS})
rclcpp_components_register_node(lidar_components
  PLUGIN "perf_lab::FakeLidar" EXECUTABLE fake_lidar)   # 同时生成可单独运行的 fake_lidar
install(TARGETS lidar_components ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
```

📄 `rclcpp_components_register_node` 注册组件**并**生成独立可执行文件；`rclcpp_components_register_nodes`（复数）只注册，不生成可执行文件。

### 三种用法

```bash
# 1. 运行时加载（命令行，底层是容器的 load_node 服务）📄
ros2 run rclcpp_components component_container
ros2 component types                                   # 列出已注册的组件
ros2 component load /ComponentManager perf_lab perf_lab::FakeLidar \
  --node-name front_lidar --node-namespace /robot1 -p bytes:=1000 \
  -e use_intra_process_comms:=true
ros2 component list
ros2 component unload /ComponentManager 1

# 2. 单独运行（register_node 生成的可执行文件）
ros2 run perf_lab fake_lidar

# 3. launch（最常用）
ros2 launch perf_lab lidar_container_launch.xml ipc:=true
```

[lidar_container_launch.xml](../../labs/perf_lab/launch/lidar_container_launch.xml)（🔍 实测可运行）：

```xml
<node_container pkg="rclcpp_components" exec="component_container" name="lidar_container" namespace="">
  <composable_node pkg="perf_lab" plugin="perf_lab::FakeLidar" name="fake_lidar">
    <param name="bytes" value="$(var bytes)"/>
    <extra_arg name="use_intra_process_comms" value="$(var ipc)"/>
  </composable_node>
  <composable_node pkg="perf_lab" plugin="perf_lab::CloudFilter" name="cloud_filter">
    <extra_arg name="use_intra_process_comms" value="$(var ipc)"/>
  </composable_node>
</node_container>
```

📄 容器已经由别的 launch 文件启动时，用 `<load_composable_node target="容器名">`（Python：`LoadComposableNodes`）往里加组件。

📄 `-e` / `extra_arg` 支持：`use_intra_process_comms`（默认 false）、`forward_global_arguments`、`enable_rosout`、`start_parameter_services`、`start_parameter_event_publisher`、`use_clock_thread`（默认都是 true）、`enable_logger_service`（默认 false）。

📄 还有一种"编译时组合"：自己写 `main`，把几个组件对象加进同一个执行器（`manual_composition`）。这种方式和 `dlopen_composition` 加载的组件都**不会**出现在 `ros2 component list` 里。

## 4. 常见坑

🔍 以下结果来自[组件与容器练习](../../labs/component-container.md)的实测。

1. **构造函数里做耗时操作，卡住整个容器。** 📄 组件不拥有线程，构造函数里不该阻塞。🔍 在单线程容器里加载一个构造函数睡 6 s 的组件，同一容器里 `cloud_filter` 的统计输出**中断了 6 秒**，期间一条消息都没处理。换成 `--executor-type multi-threaded` 后，其他组件照常运行。💡 注意：把等硬件、加载地图挪到[生命周期节点](../configuration/lifecycle-node.md)的 `on_configure` 或定时器回调里，只是让**加载**不再卡住；这些回调同样在容器的执行器线程里运行，单线程容器照样会停住。真正的解法是：耗时操作放进单独的线程（或异步接口），完成后再通知节点；或者至少用多线程容器，并把这类回调放进单独的回调组。
2. **原来的 `main` 里用的是多线程执行器。** 📄 改成组件后 `main` 没了，要记得给容器加 `--executor-type multi-threaded`，否则原来依赖并行的回调组设计会失效（见[回调组](callback-group.md)）。
3. **容器的命名空间不会传给组件。** 🔍 容器用 `-r __ns:=/robot1` 启动后，加载的 `FakeLidar` 仍是 `/fake_lidar`，发布 `/cloud`；只有加 `--node-namespace /robot1` 的组件才在 `/robot1` 下。📄 文档原话："Namespace remappings of the container do not affect loaded components." 🔍 launch 里也一样：`<node exec="component_container" namespace="robot2"/>` 再用 `load_composable_node` 加载的组件是 `/cloud_filter`。但 `push_ros_namespace` 会作用到同一组里 `node_container` 下的 `composable_node`（实测得到 `/robot1/fake_lidar`），因为它改的是 launch 的命名空间，而不是容器进程的。
4. **命名空间不一致，组件之间静默断开。** 🔍 同一个容器里，`--node-namespace /robot1` 的 `cloud_filter` 订阅的是 `/robot1/cloud`，而发布者在 `/cloud`，结果一直 `no cloud received`，没有任何报错。
5. **进程内通信默认关闭。** 📄 `use_intra_process_comms` 默认 false。🔍 不开时同一容器里的消息也会复制（订阅者收到的 `data` 地址不同，延迟约 3 ms 对 0.7 ms）。每个组件都要单独加 `extra_arg`。
6. **找不到组件。** 📄 运行容器的终端必须 source 了组件所在的工作空间；`ros2 component types` 里看不到就是没注册或没 source。
7. **组件头文件要求 C++20。** 🔍 Rolling 的 `rclcpp_components/register_node_macro.hpp` 会包含 `class_loader`，后者用了 C++20 的 `requires` 和 concepts。用 CMake 链接 `rclcpp_components::component` 时编译选项会自动变成 `-std=gnu++20`（实测 `flags.make`）；自己用 `-std=c++17` 编译会报 `'requires' does not name a type`。
8. **一个组件崩溃，整个容器陪葬。** 💡 段错误会带走同一进程里所有组件。驱动类、还不稳定的代码可以单独放一个容器。

## 5. 与其他概念的关系

- [执行器](executor.md)：容器类型就是执行器类型
- [回调组](callback-group.md)：多线程容器里回调组才有意义
- [进程内通信与零拷贝](intra-process.md)：组件放进同一进程后才能用
- [launch 启动系统](../configuration/launch.md)：`node_container`、`composable_node`、`load_composable_node`
- [生命周期节点](../configuration/lifecycle-node.md)：生命周期节点也可以做成组件
- 练习：[组件与容器](../../labs/component-container.md)
- 上一级：[总览脑图](../../map/overview.md)

## 6. 自测题（先自己答，再看[答案](../../answers/component-answers.md)）

1. 同事把一个激光驱动从普通节点改成组件。原来的 `main` 里先阻塞等待串口连上（最长 10 秒），再 `spin` 一个 `MultiThreadedExecutor`。改完之后放进 `component_container`，结果容器里其他组件启动时"卡了一会儿"，并且驱动里两个回调组的回调不再并行。分别解释两个现象，并给出改法。
2. 你在 launch 里先用 `<node pkg="rclcpp_components" exec="component_container" name="nav_container" namespace="robot1"/>` 启动容器，再用 `<load_composable_node target="/robot1/nav_container">` 加载 `controller` 和 `planner` 两个组件，都没写 `namespace`。启动后发现两个组件的话题都不在 `/robot1` 下。原因是什么？给出两种改法。
