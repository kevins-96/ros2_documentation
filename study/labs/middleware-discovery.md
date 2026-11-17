# 练习：中间件、Domain 与发现

> 适用：ROS 2 **Rolling**（Fast DDS 3.6.2）。对应卡片：[中间件 RMW 与 DDS](../cards/middleware/rmw-dds.md)、[Domain ID 与发现](../cards/middleware/domain-discovery.md)
> 代码：[mw_probe.cpp](mw_lab/src/mw_probe.cpp)、[super_client.xml](mw_lab/config/super_client.xml)
> 用时：约 50 分钟。第 5、6 部分需要 Docker

## 0. 编译

```bash
ln -s <你的仓库路径>/study/labs/mw_lab ~/study_ws/src/   # 已按 README 链接过就跳过
cd ~/study_ws && colcon build --packages-select mw_lab
source install/setup.bash
sudo apt install iproute2       # 第 2 部分要用 ss；多数系统已自带
```

`mw_probe` 是本批练习的"探针"：

| 参数 | 说明 |
|---|---|
| `mode` | `pub` 发、`sub` 收 |
| `topic` | 默认 `probe`（解析成 `/probe`） |
| `rate_hz`、`payload_bytes`、`text` | 发布频率、填充字节数、消息开头的文字 |
| `reliable` | 默认 `true`；`false` 是 best effort |

启动时打印自己的 RMW、Domain、发现配置、安全配置和 Fast DDS 环境变量。订阅端每秒打印一行：

```text
got 5 in last 1 s (total 15, lost 0, last #24) publishers in graph=1 matched=1
```

`publishers in graph` 来自发现信息（"我知道有几个发布者"），`matched` 是 DDS 层真正配对成功的发布者数。两个数不一样，就是"看得见、连不上"。

> 本练习里多次用到"另开一个终端"。所有终端都要 `source install/setup.bash`，并且**不要**带着上一步 `export` 的变量。

## 1. 我用的是哪个 RMW

```bash
ros2 doctor --report | grep -A1 "RMW MIDDLEWARE"
ros2 run mw_lab mw_probe --ros-args -p mode:=sub     # 看第一行，Ctrl-C
RMW_IMPLEMENTATION=rmw_cyclonedds_cpp ros2 run mw_lab mw_probe; echo "rc=$?"
ls /opt/ros/rolling/lib | grep -E '^librmw_[a-z]+_(cpp|c)\.so'
```

| # | 问题 | 预测 | 实测 |
|---|---|---|---|
| 1 | 不设 `RMW_IMPLEMENTATION` 时用的是哪个 | | |
| 2 | 设成一个没装的 RMW 会怎样？退出码？ | | |
| 3 | 你的机器上装了几个 RMW | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

2026-11-17 在 `ros:rolling-ros-base` 容器里实测：

1. `rmw=rmw_fastrtps_cpp`。📄 装了 Fast DDS 就默认用它。
2. 立即退出，`rc=1`：

   ```text
   [ERROR] [rcl]: Error getting RMW implementation identifier / RMW implementation not installed
   (expected identifier of 'rmw_cyclonedds_cpp'), with error message 'failed to load shared library
   'librmw_cyclonedds_cpp.so' due to dlopen error: librmw_cyclonedds_cpp.so: cannot open shared
   object file: No such file or directory ...', exiting with 1.
   ```

   📄 如果编译时只链接了一个 RMW（源码编译只装一个时），报的是另一种：`Expected RMW implementation identifier of 'rmw_connextdds' but instead found 'rmw_fastrtps_cpp', exiting with 102.`
3. 这个镜像只有 `librmw_fastrtps_cpp.so`。想试 Cyclone DDS 或 Zenoh 要另装 `ros-rolling-rmw-cyclonedds-cpp` / `ros-rolling-rmw-zenoh-cpp`。

</details>

## 2. Domain 和端口

终端 A、B、C 分别：

```bash
ROS_DOMAIN_ID=0 ros2 run mw_lab mw_probe --ros-args -p mode:=pub      # A
ROS_DOMAIN_ID=0 ros2 run mw_lab mw_probe --ros-args -p mode:=sub      # B
ROS_DOMAIN_ID=5 ros2 run mw_lab mw_probe --ros-args -p mode:=pub      # C
```

终端 D：

```bash
ss -uanp | grep -E ':(7[0-9]{3}|8[0-9]{3}) ' | awk '{print $4, $NF}' | sort
ros2 daemon stop
```

先用卡片里的公式算出 A、B、C 应该占用哪些端口，再对照实测。

| # | 问题 | 预测 | 实测 |
|---|---|---|---|
| 1 | A（Domain 0，第一个参与者）占用哪些端口 | | |
| 2 | B 的端口和 A 有什么不同 | | |
| 3 | C（Domain 5）占用哪些端口 | | |
| 4 | B 收得到 C 吗 | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

实测（括号里是谁占用）：

```text
0.0.0.0:7400  A、B 都有（多播，同一 Domain 的进程共享）
0.0.0.0:7410  A    0.0.0.0:7411  A        ← 参与者 0：发现单播 7410，用户单播 7411
0.0.0.0:7412  B    0.0.0.0:7413  B        ← 参与者 1：7412、7413
0.0.0.0:8650  C    0.0.0.0:8660  C    0.0.0.0:8661  C   ← Domain 5：7400 + 250×5 = 8650
0.0.0.0:7000  A    0.0.0.0:7001  B    ...                ← Fast DDS 共享内存传输的端口
```

1. 7400、7410、7411。
2. 参与者编号 +1，单播端口 +2：7412、7413。
3. 8650、8660、8661。
4. 收不到：`publishers in graph=0 matched=0`，连"看见"都没有。
5. 另外：`7401`（用户数据多播）没有被占用，Fast DDS 默认不用它。

🔍 如果你先跑过 `ros2 topic list` 等命令，还会看到 `ros2-daemon` 也占了一组端口（它也是一个参与者，会把后来的进程的参与者编号往后挤）。第 6 批 gdb 里看到的线程名 `dds.udp.7400`、`dds.udp.7410`、`dds.udp.7411` 就是这里的端口。

</details>

## 3. daemon 记住的是"启动它那一刻"的配置

```bash
ros2 daemon stop
ps -eo args | grep ros2-daemon | grep -v grep          # 应该没有
ros2 topic list > /dev/null
ROS_DOMAIN_ID=1 ros2 topic list > /dev/null
ps -eo args | grep ros2-daemon | grep -v grep
```

| # | 问题 | 预测 | 实测 |
|---|---|---|---|
| 1 | 现在有几个 daemon？命令行参数里记了什么 | | |
| 2 | `ros2 daemon stop` 会停掉哪个 | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

1. 两个，每个 Domain 一个：

   ```text
   /usr/bin/python3 -c from ros2cli.daemon.daemonize import main; main() --name ros2-daemon --ros-domain-id 0 --rmw-implementation rmw_fastrtps_cpp
   /usr/bin/python3 -c from ros2cli.daemon.daemonize import main; main() --name ros2-daemon --ros-domain-id 1 --rmw-implementation rmw_fastrtps_cpp
   ```

   Domain ID 和 RMW 写在命令行里，换这两个会自动用另一个 daemon。**但 `ROS_DISCOVERY_SERVER`、`ROS_AUTOMATIC_DISCOVERY_RANGE`、`FASTDDS_DEFAULT_PROFILES_FILE` 这些不在里面**：daemon 用的是第一次启动它时的环境，你后来改了环境变量，`ros2 topic list` 问的还是那个旧 daemon。第 6 部分会撞上这个坑。
2. 只停当前 `ROS_DOMAIN_ID` 对应的那个。🔍 实测做完一轮实验后，Domain 1 和 101 的 daemon 一直挂在后台。

</details>

## 4. 发现范围：同一台机器

每一行在两个终端里分别带上环境变量启动 pub 和 sub，看 sub 的输出。

| # | pub 端环境 | sub 端环境 | 预测：收得到吗 | 实测 |
|---|---|---|---|---|
| 1 | （不设） | （不设） | | |
| 2 | `ROS_AUTOMATIC_DISCOVERY_RANGE=LOCALHOST` | （不设） | | |
| 3 | `ROS_AUTOMATIC_DISCOVERY_RANGE=OFF` | （不设） | | |
| 4 | `ROS_AUTOMATIC_DISCOVERY_RANGE=OFF ROS_STATIC_PEERS=127.0.0.1` | 同左 | | |
| 5 | `ROS_AUTOMATIC_DISCOVERY_RANGE=localhost`（小写） | 同左 | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

1. 能（默认 `SUBNET`）。
2. 能。同一台机器上 `LOCALHOST` 和 `SUBNET` 互通。
3. 不能。sub 端 `graph=0`。
4. **不能**。`OFF` 就是"不和任何人发现"，加上静态对端也没用，和文档表格一致。两边都 `OFF` 且不设对端时，还会打印 `Warning: ROS_AUTOMATIC_DISCOVERY_RANGE=OFF with no ROS_STATIC_PEERS configured.`
5. 能，但有警告：`[WARN] [rcl]: Invalid value 'localhost' specified for 'ROS_AUTOMATIC_DISCOVERY_RANGE', assuming localhost only`。取值区分大小写，写错时按 `LOCALHOST` 处理。

</details>

## 5. 发现范围：两台"机器"

用两个容器模拟两台机器（`mw_lab` 不需要，直接用 `ros2 topic`）：

```bash
docker network create rosnet
docker run -d --name ha --network rosnet --hostname ha ros:rolling-ros-base sleep infinity
docker run -d --name hb --network rosnet --hostname hb ros:rolling-ros-base sleep infinity
docker exec ha hostname -i; docker exec hb hostname -i      # 记下两个 IP

# 每一行：ha 里发，hb 里收（-e 设环境变量）
docker exec -d -e <pub 端环境> ha bash -lc 'source /opt/ros/rolling/setup.bash; ros2 topic pub -r 5 /probe std_msgs/msg/String "{data: hi}"'
docker exec -e <sub 端环境> hb bash -lc 'source /opt/ros/rolling/setup.bash; timeout 8 ros2 topic echo --once /probe std_msgs/msg/String'
docker exec ha pkill -f "topic pub"
```

| # | ha（发） | hb（收） | 预测 | 实测 |
|---|---|---|---|---|
| 1 | 不设 | 不设 | | |
| 2 | `LOCALHOST` | `LOCALHOST` | | |
| 3 | `LOCALHOST` | 不设（`SUBNET`） | | |
| 4 | `LOCALHOST` + `ROS_STATIC_PEERS=<hb 的 IP>` | `LOCALHOST` | | |
| 5 | `LOCALHOST` + `ROS_STATIC_PEERS=hb`（主机名） | `LOCALHOST` | | |
| 6 | `OFF` + `ROS_STATIC_PEERS=<hb 的 IP>` | `ROS_STATIC_PEERS=<ha 的 IP>` | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

ha = 172.18.0.2，hb = 172.18.0.3：

1. 能。Docker 的 bridge 网络里多播是通的。
2. 不能。
3. **不能**。一边 `LOCALHOST` 一边 `SUBNET`，在同一台机器上能通（第 4 部分第 2 行），跨机器就不通。
4. 能。只要**一边**把对方写进静态对端就够了，对方不需要写回来。
5. 能，主机名也行（Docker 的内置 DNS 解析了 `hb`）。
6. 不能。只要有一边是 `OFF`，什么都救不回来。

六个结果都和文档"Node A and B running in the different hosts"表格一致。💡 实际部署的常见组合：机器人上所有节点 `LOCALHOST`（不往 Wi-Fi 上广播），调试笔记本也 `LOCALHOST` 并把机器人 IP 写进 `ROS_STATIC_PEERS`，只有笔记本和这台机器人互通，旁边的机器人看不见。

</details>

## 6. Discovery Server

继续用 ha、hb。

```bash
# ha：启动服务器，再起一个用服务器发现的发布者
docker exec -d ha bash -lc 'source /opt/ros/rolling/setup.bash; fastdds discovery -i 0 > /tmp/ds.log 2>&1'
docker exec ha cat /tmp/ds.log
docker exec -d ha bash -lc 'source /opt/ros/rolling/setup.bash; export ROS_DISCOVERY_SERVER=127.0.0.1:11811; ros2 topic pub -r 5 /probe std_msgs/msg/String "{data: hi}"'

# hb：进入交互终端，下面的命令都在 hb 里执行
docker exec -it hb bash
source /opt/ros/rolling/setup.bash
```

在 hb 里：

```bash
timeout 6 ros2 topic echo --once /probe std_msgs/msg/String                    # (a) 普通节点
ros2 topic list                                                                  # (b) 普通 CLI，会启动 daemon
export ROS_DISCOVERY_SERVER=172.18.0.2:11811
timeout 6 ros2 topic echo --once /probe std_msgs/msg/String                    # (c) 客户端
ros2 topic list                                                                  # (d) 设了变量，但 daemon 还是 (b) 那个
ros2 topic list --no-daemon --spin-time 3                                        # (e)
ros2 daemon stop; ros2 topic list                                                # (f)
```

再试一下文档的 Super Client XML（文档"ROS 2 CLI with Discovery Server"一节）和本仓库的 [super_client.xml](mw_lab/config/super_client.xml)（地址改成 172.18.0.2）：

```bash
FASTDDS_DEFAULT_PROFILES_FILE=<文档的 XML> ros2 topic list --no-daemon 2>&1 | head -3
FASTDDS_DEFAULT_PROFILES_FILE=super_client.xml ros2 topic info -v /probe --no-daemon --spin-time 5
```

最后，在 hb 里先起一个持续的订阅，再到 ha 里 `pkill -f fast-discovery-server` 杀掉服务器。

| # | 问题 | 预测 | 实测 |
|---|---|---|---|
| 1 | (a) 普通节点收得到客户端发的消息吗 | | |
| 2 | (c) 收得到吗 | | |
| 3 | (d) 和 (e)、(f) 的结果有什么不同？为什么 | | |
| 4 | 文档的 XML 能用吗 | | |
| 5 | 杀掉服务器后，已经连上的订阅还收得到吗？新启动的客户端呢 | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

`fastdds discovery -i 0` 打印 `Server started`、`GUID prefix: 44.53.00.5f.45.50.52.4f.53.49.4d.41`、`Running on: UDPv4:[0.0.0.0]:11811`。🔍 Fast DDS 3.6 的帮助里写着 `-i/--server-id` 已弃用（不写也得到同样的 GUID 前缀），文档还在用。

1. 收不到。客户端不发多播，普通节点也不认识服务器。📄 "用 Discovery Server 的节点和不用的节点互相看不见"。
2. 收得到。
3. (d) 只有 `/parameter_events`、`/rosout`，**没有 `/probe`**；(e) 和 (f) 都有 `/probe`。原因是第 3 部分说的：(b) 启动的 daemon 是普通发现，后来设的 `ROS_DISCOVERY_SERVER` 它看不到。换了发现方式，**一定要 `ros2 daemon stop`**。
4. 不能。🔍 Fast DDS 3.6 报 `XMLPARSER Error: Node 'discoveryServersList' without content`、`Error parsing '...'`，然后静默忽略这个文件。文档的 `<RemoteServer prefix="...">` 是 Fast DDS 2.x 的写法；3.x 要把 `<locator>` 直接写在 `<discoveryServersList>` 下面（见 [super_client.xml](mw_lab/config/super_client.xml)），这样写能用，`Publisher count: 1`。
5. 已经连上的继续收（实测 5 秒内从 17 条涨到 43 条）：📄 端点一旦互相发现，数据直接点对点走，不经过服务器。新启动的客户端收不到。

💡 关于 CLI 看不全图：📄 文档说 Discovery Server v2 按话题过滤发现信息，普通客户端身份的 CLI 只能看到一部分，要用 Super Client。🔍 本次简单场景里普通客户端身份的 CLI 也看到了 `/probe` 和它的发布者，没复现"看不全"；但大系统里别依赖这一点，CLI 和 daemon 用 Super Client 最稳：上面的 XML，或者 🔍 `export ROS_SUPER_CLIENT=TRUE`（Fast DDS 3.6 支持这个变量，文档没写）。两种方式都要先 `ros2 daemon stop`。

收尾：`docker rm -f ha hb && docker network rm rosnet`。

</details>

## 挑战（选做）

1. 用 `mw_probe` 的 `publishers in graph` 和 `matched` 两个数，给第 1 批 [QoS 不匹配](qos-mismatch.md) 的几种情况分类：哪些是 `graph=1 matched=0`，哪些是 `graph=0`？
2. 在 Domain 101 里连续启动 `mw_probe`，用 `ss` 观察端口号，算一算 Linux 上第几个参与者会撞进临时端口范围（`cat /proc/sys/net/ipv4/ip_local_port_range`），和卡片里"Domain 101 最多 54 个进程"对照。
3. 起两个 Discovery Server（11811、11888），按文档的"冗余"配置让客户端同时连两个，杀掉其中一个，验证新客户端仍然能互相发现。
