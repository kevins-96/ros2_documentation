# 答案：中间件 RMW 与 DDS

> 先在[卡片](../cards/middleware/rmw-dds.md)里自己写下答案再来对照。

## 题 1：换成 Cyclone 后"看得见话题、收不到消息"

**原因 1：`ros2 topic list` 问的是旧 daemon。** 笔记本之前用默认 RMW（Fast DDS）跑过 CLI，daemon 是用 Fast DDS 启动的，它和机器人同一个 RMW，当然能看到机器人的话题。`ros2 topic echo` 要自己创建订阅者，用的是你新设的 Cyclone。验证：`ps -eo args | grep ros2-daemon` 看 `--rmw-implementation`；或者 `ros2 topic list --no-daemon`，用 Cyclone 自己发现，大概率什么也看不到。

**原因 2：跨厂商 DDS 互通不保证。** 📄 Fast DDS 和 Cyclone DDS 都实现 RTPS，有时能发现对方，但数据能不能收到取决于实现细节和默认配置（类型信息、共享内存、分片大小等）。验证：笔记本上 `unset RMW_IMPLEMENTATION`（回到 Fast DDS），`ros2 daemon stop`，再 `echo`，能收到就说明是 RMW 不一致的问题。

**原因 3（和 RMW 无关但常见）：QoS 不兼容或机器人那边有 XML 配置。** 比如 `/plan` 是 best effort，CLI 默认按发布者的 QoS 订阅一般没问题，但如果机器人那边的发布者配了 Fast DDS 的分区，换哪个 RMW 都收不到。验证：用 `ros2 topic info -v /plan` 看 QoS；问清楚机器人上是否设了 `FASTDDS_DEFAULT_PROFILES_FILE`。

结论：**整个系统统一一个 RMW**，切换后先 `ros2 daemon stop`。

## 题 2："换成 Zenoh 就不用管 QoS 和多播"

**对的部分：**

- 📄 Zenoh 下"基本上没有不兼容的 QoS"，reliable 订阅者能收到 best effort 发布者的数据。
- 📄 Zenoh 默认不用多播发现，靠路由器 `rmw_zenohd` 的 gossip。所以在多播不通的网络里（公司 Wi-Fi、云）确实省事。

**有前提的部分：**

- 必须有路由器在运行，节点都要能连到它；路由器成了需要部署、监控的组件。没有路由器，节点能启动但互相发现不了。
- QoS 本身没有消失：`depth`、`durability`（transient local）这些语义仍然影响行为，只是不会因为"组合不兼容"而断开。
- 只是换了一种配置方式：`ROS_AUTOMATIC_DISCOVERY_RANGE`、`ROS_STATIC_PEERS` 不再生效，要改用 Zenoh 的会话和路由器配置文件（`ZENOH_SESSION_CONFIG_URI`、`ZENOH_ROUTER_CONFIG_URI`）。

**换之前检查三件事：**

1. 所有机器、所有容器、所有工具（包括调试笔记本、CI、rosbag 回放机）都能换成同一个 RMW。DDS 和 Zenoh 之间完全不通，换一半就是全断。
2. 用到的功能在 Zenoh 下是否有对应：安全（要用 `zenoh_security_tools` 重新生成配置）、Fast DDS XML 里的定制（分区、Discovery Server）都要重新做。
3. 性能和资源：在目标网络上用自己的消息（尤其是点云、地图这种大消息）实测延迟和吞吐，💡 不要只看宣传；同时确认发行版（🔍 rmw_zenoh README：Humble 的版本和更新的发行版不兼容）。
