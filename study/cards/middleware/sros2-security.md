---
tags: [ros2, security, sros2, dds-security, 安全]
distro: rolling
status: batch-7
aliases: [安全, SROS2, sros2, DDS Security, 密钥库, keystore, enclave, 飞地, ROS_SECURITY_ENABLE, ROS_SECURITY_STRATEGY, Enforce, ROS_SECURITY_KEYSTORE, 访问控制, permissions, governance, tcpdump 抓包]
---

# 安全 SROS2

> 适用：ROS 2 **Rolling**，Fast DDS 3.6.2，`sros2` 随 `ros-base` 安装（2026-11-17 实测）
> 来源标记：📄 仓库文档原文 · 🔍 源码或实测核实 · 💡 经验补充（可信，但请自己验证）
> 源文档：[About-Security.rst](../../../source/Developer-Tools/Launch/Security/About-Security.rst)、[Introducing-ros2-security.rst](../../../source/Developer-Tools/Launch/Security/Introducing-ros2-security.rst)、[Access-Controls.rst](../../../source/Developer-Tools/Launch/Security/Access-Controls.rst)、[Security-on-Two.rst](../../../source/Developer-Tools/Launch/Security/Security-on-Two.rst)、[The-Keystore.rst](../../../source/Developer-Tools/Build/The-Keystore.rst)、[Examine-Traffic.rst](../../../source/Developer-Tools/Introspection-and-analysis/Examine-Traffic.rst)
> 📐 设计动机：[ROS 图怎么映射到 DDS](../../design/graph-to-dds-mapping.md)：SROS2 权限文件里写的是 DDS 名字（`rt/`、`rq/`、`rr/`），安全凭据为什么从"每个节点"变成"每个进程"（enclave）

## 1. 一句话定义

SROS2 是 ROS 2 对 DDS Security 标准的封装：用一个 CA（证书颁发机构）给每个 **enclave**（安全飞地，通常一个进程或一个应用一个）签发身份证书和权限文件，节点启动时带上 `--enclave`，DDS 就会**认证**（你是谁）、**加密**（别人看不懂）、**访问控制**（你只能发/收允许的话题）。三个环境变量打开：`ROS_SECURITY_KEYSTORE`、`ROS_SECURITY_ENABLE=true`、`ROS_SECURITY_STRATEGY=Enforce`。📄

## 2. 为什么需要

- 默认的 DDS 流量是明文：🔍 抓包实测，关掉共享内存后 `lo` 上 15 个报文里直接能看到消息内容 `SECRET_PLAN_42`。同一个网络里任何人都能订阅你的 `/cmd_vel`，也能往里发。
- 打开安全后：🔍 0 个报文含明文；没有证书的节点连发布者都发现不了（`publishers in graph=0`）。
- 访问控制让"被攻破的节点"影响有限：🔍 重映射到不允许的话题，节点在**启动时**就被拒绝。

### 密钥库长什么样（🔍 `ros2 security create_keystore` + `create_enclave` 实测）

```text
ks/
├── public/                      ← 低敏感：CA 证书，随设备部署
│   ├── ca.cert.pem              CN=sros2CA，自签名，P-256 椭圆曲线，有效期 10 年
│   ├── identity_ca.cert.pem     → ca.cert.pem（链接）
│   └── permissions_ca.cert.pem  → ca.cert.pem（链接）
├── private/                     ← 高敏感：CA 私钥，**不要部署**，离线保管并备份
│   ├── ca.key.pem
│   ├── identity_ca.key.pem      → ca.key.pem
│   └── permissions_ca.key.pem   → ca.key.pem
└── enclaves/                    ← 中敏感：只部署设备需要的那几个
    ├── governance.xml / governance.p7s    整个 Domain 的规则（加密什么、是否允许未认证参与者）
    └── lab/probe_pub/
        ├── cert.pem             本 enclave 的证书（Identity CA 签发）
        ├── key.pem              本 enclave 的私钥
        ├── permissions.p7s      签过名的权限（Permissions CA 签名）
        ├── permissions.xml      权限源文件，运行时不需要
        ├── governance.p7s       → ../../governance.p7s
        ├── identity_ca.cert.pem → ../../../public/identity_ca.cert.pem
        └── permissions_ca.cert.pem → ../../../public/permissions_ca.cert.pem
```

📄 运行时每个 enclave 需要 6 个文件，文件名由 DDS Security 标准规定。DDS 允许身份和权限用两个不同的 CA，sros2 默认用同一个。

🔍 默认 governance：发现、存活性、数据、元数据 `ENCRYPT`，RTPS `SIGN`，`allow_unauthenticated_participants=false`。默认 permissions：允许所有 `rt/*` 话题和所有服务、动作，但**只在创建时的 `ROS_DOMAIN_ID` 里**。

### 三个环境变量的组合（📄 文档测验，🔍 实测）

| `ROS_SECURITY_ENABLE` | `ROS_SECURITY_STRATEGY` | enclave 文件 | 结果 |
|---|---|---|---|
| 不设 | — | — | 不安全模式，收不到安全节点的任何东西 |
| `true` | 不设（Permissive） | 找不到 | **照样启动，不安全模式**，只有一段不起眼的错误提示 |
| `true` | `Enforce` | 找不到 | 启动失败：`SECURITY ERROR: directory '...' does not exist.` |
| `True`（大写） | `Enforce` | 正确 | 等于没开 |
| `true` | `Enforce` | 正确 | 安全通信，日志有 `Found security directory: ...` |

## 3. 最小用法

```bash
# 1. 建密钥库和 enclave（在组织内部的机器上做）
ros2 security create_keystore ks
ros2 security create_enclave ks /lab/probe_pub
ros2 security create_enclave ks /lab/probe_sub

# 2. （可选）按策略收紧权限：policy 里写 ROS 名字，create_permission 翻译成 DDS 名字并签名
ros2 security create_permission ks /lab/probe_pub policy.xml
ros2 security create_permission ks /lab/probe_sub policy.xml

# 3. 运行
export ROS_SECURITY_KEYSTORE=$PWD/ks ROS_SECURITY_ENABLE=true ROS_SECURITY_STRATEGY=Enforce
ros2 run mw_lab mw_probe --ros-args -p mode:=pub -e /lab/probe_pub      # -e 即 --enclave

# 4. CLI 也要身份：用覆盖变量，且不用 daemon
export ROS_SECURITY_ENCLAVE_OVERRIDE=/lab/probe_sub
ros2 topic list --no-daemon --spin-time 3

# 5. 检查签名
openssl smime -verify -in ks/enclaves/governance.p7s -CAfile ks/public/permissions_ca.cert.pem -out /dev/null
```

policy 片段（完整文件见 [policy.xml](../../labs/mw_lab/config/policy.xml)，📄 格式参考 sros2 仓库的 `sample.policy.xml`）：

```xml
<enclave path="/lab/probe_pub">
  <profiles>
    <profile ns="/" node="mw_probe">
      <!-- 还要加上每个节点都需要的：rosout、/parameter_events、/clock、参数服务、类型描述服务 -->
      <topics publish="ALLOW">
        <topic>probe</topic>
      </topics>
    </profile>
  </profiles>
</enclave>
```

📄 手改 `permissions.xml` 的话，话题要写 DDS 名字（`rt/chatter`、`rt/rosout`、`rt/parameter_events`，服务是 `rq/...Request`、`rr/...Reply`），默认 `<default>DENY</default>`；改完必须用 Permissions CA 的私钥重新签名：

```bash
openssl smime -sign -text -in permissions.xml -out permissions.p7s \
  --signer permissions_ca.cert.pem -inkey ks/private/permissions_ca.key.pem
```

🔍 这条命令（包括双横线的 `--signer`）在容器的 OpenSSL 里实测可用，签完 `openssl smime -verify` 打印 `Verification successful`。

## 4. 常见坑

1. **不设 `Enforce`，配错了会悄悄降级。** 📄🔍 enclave 路径写错、密钥库不对，节点照样启动并以不安全模式运行，然后收不到任何安全节点的数据。🔍 实测没有一条明确的警告，只有一段 rcutils "错误状态被覆盖"的提示。部署时务必 `ROS_SECURITY_STRATEGY=Enforce`。
2. **`ROS_SECURITY_ENABLE` 区分大小写。** 📄🔍 只认 `true`；`True`、`1` 都等于没开。
3. **permissions 绑定 Domain ID。** 🔍 `create_enclave` 把当时的 `ROS_DOMAIN_ID` 写进 `<domains><id>`；换到 Domain 5 启动，报 `Not found topic access rule for topic ros_discovery_info`，进程退出。换 Domain 就要重新生成 permissions。
4. **改一个字节签名就失效。** 📄🔍 往 `permissions.p7s` 里加一个空格：`openssl` 报 `Verification failure`，节点报 `Participant is not allowed with its own permissions file.` 后启动失败。只影响这一个 enclave。
5. **访问控制按名字匹配，改名就要改策略。** 🔍 用了最小权限 policy 后，`-r probe:=other` 报 `rt/other topic not found in allow rule`；`-r __node:=spy` 报 `rq/spy/get_parametersRequest topic not found in allow rule`（参数服务的名字跟着节点名变了）。都在启动时 `abort`（退出码 250）。💡 launch 里改命名空间、节点名，policy 要同步。
6. **policy 漏了节点的"基础设施"话题，节点起不来。** 🔍 每个 rclcpp 节点都会创建 `rosout`、`/parameter_events`、`/clock`、6 个参数服务和 `~/get_type_description` 服务；sros2 仓库把它们放在 `test/policies/common/node/*.xml`，用 XInclude 引用。
7. **CLI 用 daemon 看不到安全的图。** 📄 daemon 可能没有 enclave；用 `ROS_SECURITY_ENCLAVE_OVERRIDE` + `--no-daemon --spin-time 3`，并给足发现时间。
8. **加密不隐藏"谁在网络上"。** 🔍 打开安全后抓包仍能看到 `DDS:Auth:PKI-DH:1.0`、`DDS:Access:Permissions:1.0` 和证书里的 enclave 名字（明文）。📄 文档推荐的论文讲的就是对安全 DDS 系统的网络侦察。
9. **抓包看不到数据不代表加密了。** 📄🔍 Fast DDS 默认同机走共享内存，`lo` 上根本没有数据报文（实测 259 个报文全是发现和控制报文，0 个含内容）。要验证加密，先 `FASTDDS_BUILTIN_TRANSPORTS=UDPv4` 或换两台机器。
10. **私钥文件权限。** 🔍 容器里以 root 创建时 `umask` 为 `0000`，`key.pem`、`ca.key.pem` 都是 `-rw-rw-rw-`。💡 部署时 `chmod 600`，`enclaves/` 只读（📄 Deployment Guidelines 也建议只读）。
11. **跨厂商没有安全互通；源码编译的 Fast DDS 要开 `-DSECURITY=ON`。** 📄 二进制包已经开了。Zenoh 用自己的 `zenoh_security_tools` 生成配置，并需要为路由器准备 enclave。
12. **文档的小错。** 📄 [Security-on-Two.rst](../../../source/Developer-Tools/Launch/Security/Security-on-Two.rst) 在 `demo_keystore` 目录下 `scp -r talker ...`，但 enclave 实际在 `enclaves/talker_listener/talker`；Bob 上也只建了 `~/sros2_demo`，没建 `demo_keystore`。🔍 安全教程里的 `ros2 run rmw_zenoh_cpp zenohd` 应为 `rmw_zenohd`（见 [中间件 RMW 与 DDS](rmw-dds.md)）。

## 5. 与其他概念的关系

- [中间件 RMW 与 DDS](rmw-dds.md)：安全是 DDS Security 插件实现的，跨厂商不通
- [Domain ID 与发现](domain-discovery.md)：发现报文加密；permissions 绑定 Domain
- [Docker 部署](docker-deploy.md)：哪些密钥文件进镜像、进设备
- [启动参数与重映射](../configuration/node-arguments.md)：`--enclave` / `-e`，以及重映射对访问控制的影响
- [命令行工具](../observability/cli-tools.md)：daemon 和 `--no-daemon`
- 练习：[SROS2 密钥库、强制模式与访问控制](../../labs/sros2-keystore.md)
- [ROS 图怎么映射到 DDS](../../design/graph-to-dds-mapping.md)（📐 设计动机）：权限文件里的 DDS 话题名从哪来
- 上一级：[总览脑图](../../map/overview.md)

## 6. 自测题（先自己答，再看[答案](../../answers/sros2-security-answers.md)）

1. 现场同事反馈："开了安全以后规划器收不到定位了，但规划器自己启动正常、日志里没有报错。" 现场只设了 `ROS_SECURITY_ENABLE` 和 `ROS_SECURITY_KEYSTORE`。列出至少三种可能原因，说明为什么"启动正常"本身就是线索，以及怎么改配置让这类问题在启动时就暴露出来。
2. 你要给车队里 20 台机器人部署 SROS2，每台机器人上有 `driver`、`localization`、`planner` 三个进程，地面站有一个监控进程只读所有话题。设计 enclave 的划分、CA 和私钥放在哪、每台机器人上部署哪些文件，并说明新增一台机器人和"`planner` 被攻破"这两种情况各需要做什么。
