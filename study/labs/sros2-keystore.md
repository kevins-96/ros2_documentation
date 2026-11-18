# 练习：SROS2 密钥库、强制模式与访问控制

> 适用：ROS 2 **Rolling**（Fast DDS 3.6.2，`sros2` 已随 `ros-base` 安装）。对应卡片：[安全 SROS2](../cards/middleware/sros2-security.md)
> 代码：[mw_probe.cpp](mw_lab/src/mw_probe.cpp)、[policy.xml](mw_lab/config/policy.xml)
> 用时：约 45 分钟。编译见[中间件、Domain 与发现](middleware-discovery.md)第 0 部分；第 5 部分需要 `tcpdump`

所有命令在同一个目录里执行：`mkdir -p ~/sros2_lab && cd ~/sros2_lab`。每个终端先定义一个函数，下面的 `sec;` 就是"打开安全并强制"：

```bash
sec(){ export ROS_SECURITY_KEYSTORE=~/sros2_lab/ks ROS_SECURITY_ENABLE=true ROS_SECURITY_STRATEGY=Enforce; }
```

## 1. 建密钥库，看看里面有什么

```bash
ros2 security create_keystore ks
ros2 security create_enclave ks /lab/probe_pub
ros2 security create_enclave ks /lab/probe_sub
ros2 security list_enclaves ks
find ks | sort; ls -l ks/public ks/private ks/enclaves/lab/probe_pub
openssl x509 -in ks/public/ca.cert.pem -noout -subject -issuer -dates
openssl smime -verify -in ks/enclaves/governance.p7s -CAfile ks/public/permissions_ca.cert.pem -out /dev/null
grep -E "protection_kind|allow_unauthenticated|access_control" ks/enclaves/governance.xml
grep -A2 "<domains>" ks/enclaves/lab/probe_pub/permissions.xml
```

| # | 问题 | 预测 | 实测 |
|---|---|---|---|
| 1 | 每个 enclave 目录里几个文件？哪些是链接 | | |
| 2 | 有几个 CA？证书有效期多久 | | |
| 3 | `private/` 里的私钥文件权限是什么 | | |
| 4 | 默认 governance 加密了哪些东西 | | |
| 5 | 默认 permissions 允许什么，限定在哪个 Domain | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

2026-11-17 在 `ros:rolling-ros-base` 容器里实测：

1. 7 个：`cert.pem`、`key.pem`、`permissions.p7s`、`permissions.xml` 是这个 enclave 自己的；`governance.p7s`、`identity_ca.cert.pem`、`permissions_ca.cert.pem` 是指向公共文件的**符号链接**。📄 运行时必需的是 6 个，`permissions.xml` 只是用来重新签名的源文件。
2. 实际只有一个：`public/identity_ca.cert.pem` 和 `permissions_ca.cert.pem` 都链接到 `ca.cert.pem`，`private/` 同理。`subject=CN=sros2CA`、`issuer=CN=sros2CA`（自签名），有效期 10 年（2026-11-17 到 2036-11-14）。`openssl smime -verify` 打印 `Verification successful`。
3. 🔍 容器里是 `-rw-rw-rw-`（所有人可读写）：容器的 root 用户 `umask` 是 `0000`。💡 部署前把 `private/` 移走，`key.pem` 改成 `chmod 600`。
4. 发现、存活性、数据、元数据都是 `ENCRYPT`，RTPS 报文 `SIGN`；`allow_unauthenticated_participants` 为 `false`，读、写、加入都做访问控制。
5. 允许发布、订阅 `rt/*`（所有话题），以及所有服务和动作的 `rq/`、`rr/` 话题，`<default>DENY</default>`；**限定在 `<id>0</id>`**，也就是创建时的 `ROS_DOMAIN_ID`（实测 `ROS_DOMAIN_ID=5 ros2 security create_enclave ...` 得到 `<id>5</id>`）。

</details>

## 2. 安全开关的四种组合

终端 A（安全的发布端，一直开着）：

```bash
sec; ros2 run mw_lab mw_probe --ros-args -p mode:=pub -e /lab/probe_pub
```

终端 B 依次试（每次 Ctrl-C 后换下一行；先 `unset` 掉所有 `ROS_SECURITY_*`）：

```bash
ros2 run mw_lab mw_probe --ros-args -p mode:=sub                                         # (a)
ROS_SECURITY_KEYSTORE=$PWD/ks ROS_SECURITY_ENABLE=true \
  ros2 run mw_lab mw_probe --ros-args -p mode:=sub -e /lab/nobody                        # (b)
sec; ros2 run mw_lab mw_probe --ros-args -p mode:=sub -e /lab/nobody                     # (c)
ROS_SECURITY_KEYSTORE=$PWD/ks ROS_SECURITY_ENABLE=True ROS_SECURITY_STRATEGY=Enforce \
  ros2 run mw_lab mw_probe --ros-args -p mode:=sub -e /lab/probe_sub                     # (d)
sec; ros2 run mw_lab mw_probe --ros-args -p mode:=sub -e /lab/probe_sub                  # (e)
```


| # | 情况 | 预测：节点能启动吗？收得到吗？ | 实测 |
|---|---|---|---|
| (a) | 不开安全 | | |
| (b) | 开了安全，但 enclave 不存在，没设 Enforce | | |
| (c) | 同上，设了 Enforce | | |
| (d) | `ROS_SECURITY_ENABLE=True`（大写 T） | | |
| (e) | 全部正确 | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

- (a) 能启动，收不到：`publishers in graph=0`。加密的发现报文它看不懂，连发布者都看不见。
- (b) **能启动**，以不安全模式运行，收不到。🔍 没有一条清楚的警告，只有一段 rcutils 的"错误状态被覆盖"的提示，里面夹着 `SECURITY ERROR: directory '.../enclaves/lab/nobody' does not exist.`。很容易被当成噪音忽略。
- (c) 启动失败：`failed to initialize rcl: SECURITY ERROR: directory '/.../ks/enclaves/lab/nobody' does not exist., at ./src/rcl/security.c:207`。📄 这就是 `Enforce` 的意义：配错了就不启动，而不是悄悄降级。
- (d) 等于没开安全，和 (a) 一样。📄 `ROS_SECURITY_ENABLE` 区分大小写，只认 `true`。
- (e) 能启动，日志里有 `[rcl]: Found security directory: .../enclaves/lab/probe_sub`，每秒收到 5 条。

💡 部署时三件事一起做：`ROS_SECURITY_STRATEGY=Enforce`；启动脚本里检查 `ROS_SECURITY_ENABLE` 的值是不是正好 `true`；监控里看节点的 `Found security directory` 日志。

</details>

## 3. 签名被破坏、Domain 不对

```bash
cp ks/enclaves/lab/probe_sub/permissions.p7s /tmp/p7s.bak
sed -i 's#<default>DENY</default>#<default>DENY</default> #' ks/enclaves/lab/probe_sub/permissions.p7s   # 加一个空格
openssl smime -verify -in ks/enclaves/lab/probe_sub/permissions.p7s -CAfile ks/public/permissions_ca.cert.pem -out /dev/null
sec; ros2 run mw_lab mw_probe --ros-args -p mode:=sub -e /lab/probe_sub
cp /tmp/p7s.bak ks/enclaves/lab/probe_sub/permissions.p7s

# A、B 都换到 Domain 5（enclave 是在 Domain 0 下创建的）
sec; ROS_DOMAIN_ID=5 ros2 run mw_lab mw_probe --ros-args -p mode:=pub -e /lab/probe_pub
sec; ROS_DOMAIN_ID=5 ros2 run mw_lab mw_probe --ros-args -p mode:=sub -e /lab/probe_sub
```

| # | 问题 | 预测 | 实测 |
|---|---|---|---|
| 1 | 只加了一个空格，`openssl` 和节点分别怎么说 | | |
| 2 | 发布端 A 受影响吗 | | |
| 3 | 换到 Domain 5 会怎样 | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

1. `openssl` 报 `Verification failure` 和 `PKCS7_signatureVerify:digest failure`。节点**启动失败**（`rc=250`）：`[SECURITY Error] Participant is not allowed with its own permissions file.`、`PKCS7 data verification failed`、`Cannot create participant due to initialization error`。📄 签过名的文件改一个字节签名就失效。
2. 不受影响，它用的是另一个 enclave 的 `permissions.p7s`。
3. 两边都**启动失败**（`rc=250`），日志 `[SECURITY Error] Not found topic access rule for topic ros_discovery_info`、`Error checking creation of local writer`：permissions 里只写了 `<id>0</id>`，在 Domain 5 里连 rmw 内部用的 `ros_discovery_info` 话题都不许建。💡 换 Domain 时要在新的 `ROS_DOMAIN_ID` 下重新生成 permissions（或者自己写 policy，再用 `create_permission` 签名）。

</details>

## 4. 访问控制：只许发 `probe`

[policy.xml](mw_lab/config/policy.xml) 给两个 enclave 写了最小权限：发布端只能发布 `probe`，订阅端只能订阅 `probe`，外加每个节点都需要的日志、参数事件、时钟、参数服务。

```bash
ros2 security create_permission ks /lab/probe_pub ~/study_ws/src/mw_lab/config/policy.xml
ros2 security create_permission ks /lab/probe_sub ~/study_ws/src/mw_lab/config/policy.xml
grep "<topic>" ks/enclaves/lab/probe_pub/permissions.xml | head -12
```

重启终端 A 的发布端，然后在终端 B：

```bash
sec; ros2 run mw_lab mw_probe --ros-args -p mode:=sub -e /lab/probe_sub                       # (a)
sec; ros2 run mw_lab mw_probe --ros-args -p mode:=sub -e /lab/probe_sub -r probe:=other       # (b)
sec; ros2 run mw_lab mw_probe --ros-args -p mode:=sub -e /lab/probe_pub                       # (c)
sec; ros2 run mw_lab mw_probe --ros-args -p mode:=sub -e /lab/probe_sub -r __node:=spy        # (d)
```

| # | 问题 | 预测 | 实测 |
|---|---|---|---|
| 1 | `permissions.xml` 里的话题名长什么样 | | |
| 2 | (b)(c)(d) 分别怎么失败？失败发生在什么时候 | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

1. DDS 名字：话题 `probe` 变成 `rt/probe`，参数服务 `~/get_parameters` 变成 `rq/mw_probe/get_parametersRequest` 和 `rr/mw_probe/get_parametersReply`。📄 手写 permissions 时要用这种名字（规则见设计文档"Topic and Service Names"）。
2. 三个都是**启动时**就失败，进程退出（`rc=250`，被 `abort`）：
   - (b) `[SECURITY Error] rt/other topic not found in allow rule.` → `could not create subscription: create_datareader() could not create data reader`
   - (c) `rt/probe topic not found in allow rule.`：发布端的 enclave 不允许订阅 `probe`。
   - (d) `rq/spy/get_parametersRequest topic not found in allow rule.` → `could not create service: create_service() failed to create request DataReader`。改了节点名，参数服务的名字跟着变，policy 里是按 `node="mw_probe"` 展开的。

   💡 这正是访问控制想要的效果：一个被攻破的节点不能靠重映射去订阅别的话题。代价是 launch 里改名、改命名空间都要同步改 policy。

</details>

## 5. 抓包看加密

先 `sudo apt install tcpdump`。payload 用一个容易搜的字符串。

```bash
# 终端 C：抓 lo 上 Domain 0 的端口
sudo tcpdump -i lo -U -w /tmp/c.pcap 'udp portrange 7400-7500'
```

按下表启动 A、B（`-p text:=SECRET_PLAN_42`），各跑 5 秒后停掉 tcpdump，统计：

```bash
tcpdump -r /tmp/c.pcap | wc -l
tcpdump -r /tmp/c.pcap -A | grep -c SECRET_PLAN_42
tcpdump -r /tmp/c.pcap -A | grep -o -E "DDS:Auth:PKI-DH:1.0|/lab/probe_[a-z]+" | sort | uniq -c
```

| # | A、B 的配置 | 预测：报文里看得到 `SECRET_PLAN_42` 吗 | 实测 |
|---|---|---|---|
| 1 | 不开安全，默认传输 | | |
| 2 | 不开安全，`FASTDDS_BUILTIN_TRANSPORTS=UDPv4` | | |
| 3 | 开安全（`sec;`、`-e`），`FASTDDS_BUILTIN_TRANSPORTS=UDPv4` | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

实测（用 `ros2 topic pub/echo` 做的，`mw_probe` 结果相同）：

1. 259 个报文，**0 个**含明文。不是因为加密，而是数据走了共享内存，根本没上 `lo`；网卡上只有发现报文。📄 文档特意提醒：Fast DDS 默认用共享内存，要抓包得换机器或关掉共享内存。
2. 254 个报文，15 个含明文 `SECRET_PLAN_42`。
3. 526 个报文（握手和签名让报文翻倍），0 个含明文；能看到 `DDS:Auth:PKI-DH:1.0`、`DDS:Access:Permissions:1.0`，以及证书里的 **enclave 名字是明文**（实测用的 enclave 叫 `/demo/talker`、`/demo/listener`，抓包里各出现 12 次）。

💡 加密保护的是数据内容，不是"谁在网络上"：节点用了哪个 enclave、有几个参与者、用哪些端口，旁观者都能看到。📄 文档推荐的论文 *Network Reconnaissance and Vulnerability Excavation of Secure DDS Systems* 讲的就是这个。

</details>

## 挑战（选做）

1. 按文档 [Deployment-Guidelines](../../source/Developer-Tools/Build/Deployment-Guidelines/Deployment-Guidelines.rst) 的思路，写一个 `compose.yaml`：一个 `keystore-creator` 服务生成密钥库，`pub`、`sub` 两个服务各自只挂载自己的 enclave 目录和 `public/`，**不挂** `private/`。验证缺了 `private/` 节点照样能跑。
2. 用 `ros2 security generate_policy` 从运行中的 ROS 图自动生成 policy，和手写的 [policy.xml](mw_lab/config/policy.xml) 对比，看它多给了哪些权限。
3. 不用 `Enforce`、故意把 `ROS_SECURITY_KEYSTORE` 指向一个空目录，写一个启动前检查脚本，在节点启动前就发现"安全没真正开启"。
