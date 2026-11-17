# 练习：DDS 调优、XML 配置与容器网络

> 适用：ROS 2 **Rolling**（Fast DDS 3.6.2）。对应卡片：[DDS 调优与 Fast DDS XML](../cards/middleware/dds-tuning.md)、[Docker 部署](../cards/middleware/docker-deploy.md)
> 代码：[serialize_bench.cpp](mw_lab/src/serialize_bench.cpp)、[mw_probe.cpp](mw_lab/src/mw_probe.cpp)、[config/](mw_lab/config/partition_pub.xml)（`partition_pub.xml`、`partition_sub.xml`、`max_one_sub.xml`）、[compose.yaml](mw_lab/docker/compose.yaml)
> 用时：约 40 分钟。编译见[中间件、Domain 与发现](middleware-discovery.md)第 0 部分；第 4 部分需要 Docker 和 `docker compose`

下面用到 `$C` 的终端，先执行 `C=~/study_ws/install/mw_lab/share/mw_lab/config`。

## 1. 同样的路径，三种消息结构

规划器发的 `nav_msgs/Path` 是"复合类型的数组"：每个点都是一个 `PoseStamped`，各自带一个 `header`（时间戳 + `frame_id` 字符串）。`serialize_bench` 把同样 N 个位姿装进三种消息，分别计时序列化和反序列化。

```bash
ros2 run mw_lab serialize_bench
```

| # | 问题 | 预测 | 实测 |
|---|---|---|---|
| 1 | N = 10000 时三种消息的字节数，`Path` 每个点多出来的字节是什么 | | |
| 2 | 三者序列化耗时的大致比例 | | |
| 3 | 一个 10 Hz 的局部规划器每次发一条 10 万点的 `Path`，序列化在 100 ms 周期里占多少 | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

2026-11-17，4 核容器。`mw_lab` 按默认方式编译（不设 `CMAKE_BUILD_TYPE`，自己的代码不开优化；序列化代码在已编译好的 ROS 库里，不受影响）：

```text
Path         n=  10000  bytes=   720028  serialize=    788.6 us  deserialize=    710.8 us
PoseArray    n=  10000  bytes=   560028  serialize=    509.0 us  deserialize=    413.9 us
PointCloud2  n=  10000  bytes=   560189  serialize=     14.9 us  deserialize=     14.5 us

Path         n= 100000  bytes=  7200028  serialize=   5845.8 us  deserialize=   5230.3 us
PoseArray    n= 100000  bytes=  5600028  serialize=   3452.5 us  deserialize=   3299.2 us
PointCloud2  n= 100000  bytes=  5600189  serialize=    441.3 us  deserialize=    468.1 us
```

1. 72 / 56 / 56 字节每点。`Path` 多出的 16 字节是每个点自己的 `header`：时间戳 8 字节 + `frame_id` 字符串（长度 4 字节 + `"map\0"` 4 字节）。
2. 约 50 : 35 : 1（10000 点）。字节数差不多，耗时差几十倍：`PointCloud2` 的数据是一整块 `uint8[]`，序列化就是一次内存拷贝；另外两个要逐个元素、逐个字段地编码。
3. 约 5.8 ms，占 6%。订阅端反序列化还要再花 5 ms。📄 文档说的"复合类型的大数组序列化很慢，`publish()` 会卡住，`ros2 topic hz` 显示的频率偏低"就是这个。💡 规划器内部可以用 `Path`，但跨进程发大量点时，考虑降采样，或者像点云一样打包成字节数组（或"数组的结构体"：`float64[] x`、`float64[] y` ……）。

</details>

## 2. 分区：同一个话题里的"隔离"

```bash
# 终端 A
FASTDDS_DEFAULT_PROFILES_FILE=$C/partition_pub.xml ros2 run mw_lab mw_probe --ros-args -p mode:=pub
# 终端 B
FASTDDS_DEFAULT_PROFILES_FILE=$C/partition_sub.xml ros2 run mw_lab mw_probe --ros-args -p mode:=sub
# 终端 C：不加 XML
ros2 run mw_lab mw_probe --ros-args -p mode:=sub
# 终端 D
ros2 topic info -v /probe | grep -E "Node name|count"
```

然后把 B 的 `partition_sub.xml` 里的 `part2` 改成 `part1` 再试一次。

| # | 问题 | 预测 | 实测 |
|---|---|---|---|
| 1 | B 的 `graph` 和 `matched` 分别是多少 | | |
| 2 | 不加 XML 的 C 收得到吗 | | |
| 3 | `ros2 topic info` 能看出分区不同吗 | | |
| 4 | 没设 `RMW_FASTRTPS_USE_QOS_FROM_XML`，分区生效吗 | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

1. `publishers in graph=1 matched=0`：看得见，但不匹配。发布端 `matched subscriptions=0`。
2. 收不到。不设分区等于在"默认分区"里，和 `part1` 没有交集。改成 `part1` 后 B 每秒收到 5 条。
3. 看不出。CLI 显示一个发布者、两个订阅者，QoS 也都兼容。💡 所以遇到"QoS 都对、就是收不到"，记得问一句有没有人配了 XML。
4. 生效。🔍 `rmw_fastrtps` 无论如何都会按话题名加载 profile（源码 `publisher.cpp` 调用 `get_datawriter_qos_from_profile(topic_name, ...)`）；`RMW_FASTRTPS_USE_QOS_FROM_XML` 只决定"发布模式、历史内存策略、data sharing"这三项听不听 XML。profile 名必须是带 `/` 的完整话题名，写 `probe` 不会生效。

</details>

## 3. 匹配数上限

```bash
# 终端 A
FASTDDS_DEFAULT_PROFILES_FILE=$C/max_one_sub.xml ros2 run mw_lab mw_probe --ros-args -p mode:=pub
# 终端 B、C、D：依次启动三个订阅端
ros2 run mw_lab mw_probe --ros-args -p mode:=sub -r __node:=sub1
ros2 run mw_lab mw_probe --ros-args -p mode:=sub -r __node:=sub2
ros2 run mw_lab mw_probe --ros-args -p mode:=sub -r __node:=sub3
```

| # | 问题 | 预测 | 实测 |
|---|---|---|---|
| 1 | 几个订阅端收得到？是第一个启动的吗 | | |
| 2 | 收不到的订阅端，`matched` 是几 | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

1. 只有一个收得到。🔍 实测一次三个订阅端间隔 0.3 秒启动，收到的是**第二个**（`got 5 in last 1 s (total 15, lost 0, last #25)`），不一定是最先启动的，取决于发现报文先到谁。
2. **都是 `matched=1`**。订阅端觉得自己匹配上了，是发布端那边拒绝了。所以 `matched` 只说明"这一端"的状态，判断连通性最终要看数据有没有到。

</details>

## 4. 两个 host 网络的容器

先看 [compose.yaml](mw_lab/docker/compose.yaml)：两个服务都是 `network_mode: host`，一个发、一个收，Domain 42。

```bash
cd ~/study_ws/src/mw_lab/docker
docker compose up -d; sleep 8; docker compose logs sub | tail -3; docker compose down
```

再依次试注释里的三种改法（每次只改一种）：两个服务都加 `ipc: host`；两个服务都加 `FASTDDS_BUILTIN_TRANSPORTS=UDPv4`；删掉两行 `network_mode: host`。

| # | 配置 | 预测：收得到吗 | 实测（8 秒内收到几条） |
|---|---|---|---|
| 1 | 原样 | | |
| 2 | `ipc: host` | | |
| 3 | `FASTDDS_BUILTIN_TRANSPORTS=UDPv4` | | |
| 4 | 默认 bridge 网络 | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

1. **0 条**。两个容器共用宿主机的网络，Fast DDS 认为对方"在同一台机器上"，优先走共享内存；但两个容器各有自己的 `/dev/shm`，共享内存段互相看不见，于是谁也收不到谁。也没有任何报错。
2. 39 条。共享 `/dev/shm` 后共享内存能用了。📄 文档的 VS Code devcontainer 配置里写了 `--net=host --pid=host --ipc=host`，`--ipc=host` 就是为了这个。
3. 39 条。只用 UDP，不碰共享内存。
4. 40 条。bridge 网络里每个容器有自己的 IP，Fast DDS 知道它们不在同一台机器，直接走 UDP；多播在 bridge 网络里也是通的。

🔍 另外两个组合也测了：一个 host 网络容器 + 一个 bridge 容器，两个方向都能收到；一个 `--ipc host` 的容器 + 一个默认 ipc 的容器（都是 host 网络），收不到。

另外一个小坑：`command:` 写成一整行字符串时，`"{data: x}"` 里的冒号会让 YAML 解析失败（`mapping values are not allowed in this context`），compose 文件里命令要用列表形式。

</details>

## 5. 内核网络参数

```bash
sysctl net.ipv4.ipfrag_time net.ipv4.ipfrag_high_thresh net.core.rmem_max net.core.rmem_default
docker run --rm ros:rolling-ros-base sysctl -w net.ipv4.ipfrag_time=3
```

| # | 问题 | 预测 | 实测 |
|---|---|---|---|
| 1 | 四个值分别是多少？和文档说的默认值一样吗 | | |
| 2 | 容器里能改吗 | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

1. 🔍 测试机（Linux 6.18）：`ipfrag_time = 30`、`ipfrag_high_thresh = 4194304`、`rmem_max = 4194304`、`rmem_default = 212992`。📄 文档说 `ipfrag_high_thresh` 默认 256 KB，那是老内核的值；💡 Linux 4.17 起默认 4 MB。你的机器以自己读到的为准。
2. 不能：`sysctl: setting key "net.ipv4.ipfrag_time", ignoring: Read-only file system`。容器里的 `/proc/sys` 默认只读。💡 `net.*` 里按网络命名空间隔离的参数可以用 `docker run --sysctl net.ipv4.ipfrag_time=3` 在启动时设置；`net.core.rmem_max` 这类全局参数要在宿主机上改。

</details>

## 挑战（选做）

1. 给 `serialize_bench` 加第四种结构"数组的结构体"：自己定义一个 `PathSoA.msg`（`float64[] x`、`float64[] y`、`float64[] yaw`），比较它和 `PointCloud2` 的耗时。思考：为什么 `float64[]` 也能接近一次内存拷贝？
2. 用 `RMW_FASTRTPS_PUBLICATION_MODE=ASYNCHRONOUS` 和默认（同步）各跑一次 `mw_probe -p payload_bytes:=5000000`，用第 6 批的 [tracing 练习](crash-and-trace.md)方法测定时器回调时长，验证"同步模式下序列化和发送在 `publish()` 里完成"。
3. 写一个 XML，让 `/probe` 的发布端同时属于 `part1` 和 `part2`（`<names>` 里写两个 `<name>`），验证两个分区的订阅端都能收到。
