# 答案：安全 SROS2

> 先在[卡片](../cards/middleware/sros2-security.md)里自己写下答案再来对照。

## 题 1：开了安全后规划器收不到定位，但"启动正常、没有报错"

**"启动正常"本身就是线索：** 现场没设 `ROS_SECURITY_STRATEGY=Enforce`，也就是 Permissive 模式。Permissive 下安全配置有任何问题，节点**都会照常启动并以不安全模式运行**（🔍 实测只有一段不起眼的 rcutils 错误提示），然后收不到任何加密的数据。如果设了 `Enforce`，同样的错误会让节点直接启动失败，问题一眼就能看到。

**可能原因：**

1. **enclave 没找到**：规划器没带 `--enclave`（或 `-e`）参数，或路径写错、和密钥库里的目录对不上。查日志里有没有 `[rcl]: Found security directory: ...`，没有就说明没用上安全。
2. **`ROS_SECURITY_ENABLE` 的值不对**：写成了 `True`、`1`，📄🔍 只认 `true`，其他值等于没开。这种情况连上面那条提示都没有。
3. **`ROS_SECURITY_KEYSTORE` 指错了**：相对路径、`~` 没展开、在 launch 或 systemd 里环境变量没传进去。
4. **Domain 换过**：🔍 permissions 只允许创建时的 Domain；但这个在 Enforce 下会启动失败，在 Permissive 下也可能表现为收不到。
5. **签名失效或证书过期**：有人手改了 `permissions.p7s`、`governance.p7s`，或者 CA、enclave 证书过了有效期（sros2 默认 10 年，但自己签发的可能更短）。用 `openssl smime -verify ... -CAfile ...` 检查签名。
6. **定位节点那边才是有问题的一方**：规划器正常加密了，定位节点在不安全模式下运行，双方同样互相看不见。两边都要检查。

**让问题在启动时暴露：** 所有节点统一设 `ROS_SECURITY_STRATEGY=Enforce`；启动脚本里检查 `ROS_SECURITY_ENABLE` 正好等于 `true`、密钥库和 enclave 目录存在；把 `Found security directory` 日志作为启动检查项。

## 题 2：20 台机器人 + 地面站的 enclave 设计

**enclave 划分：** 每台机器人的每个进程一个 enclave，如 `/fleet/robot07/driver`、`/fleet/robot07/localization`、`/fleet/robot07/planner`；地面站一个 `/fleet/ground/monitor`。理由：权限按"最小必要"给（`driver` 只发传感器话题，`planner` 只订阅定位和地图、发布路径和速度，`monitor` 只订阅），而且每台机器人的身份独立，可以单独吊销。

**CA 和私钥：** 一个组织内部的 CA（或身份、权限分开两个 CA），私钥在离线的签发机器上，备份到安全介质；所有 policy 文件也在组织内维护，用 `ros2 security create_permission` 生成并签名。

**每台机器人上部署：** `public/`（CA 证书）、`enclaves/governance.p7s`，以及**本机三个 enclave 目录**（各含 `cert.pem`、`key.pem`、`permissions.p7s` 和三个指向公共文件的链接）。不部署 `private/`、不部署其他机器人的 enclave。地面站同理，只有 `monitor` 的 enclave。

**新增一台机器人：** 在组织内部的签发机器上为它创建三个新 enclave（按同一套 policy 生成 permissions），把这三个目录和 `public/`、`governance.p7s` 部署上去。已有的机器人不需要改动，因为它们信任的是同一个 CA。（如果 permissions 里写了具体的对端，或者用了按 Domain 的划分，要确认新机器人用的 Domain 和 policy 一致。）

**`planner` 被攻破：** 攻击者拿到了 `robot07/planner` 的 `key.pem`。

1. 影响范围有限：访问控制只允许它发布路径和速度、订阅定位和地图，它不能冒充 `driver` 发传感器数据，不能订阅其他话题（🔍 重映射到不允许的话题时节点直接启动失败），其他机器人不受影响。
2. 处置：📄 文档说 enclave 私钥丢失或被盗时"吊销这个密钥，并为 enclave 创建新身份"。为 `robot07/planner` 生成新的 `key.pem` 和 `cert.pem`，并让旧证书失效。💡 难点在"失效"：其他参与者只检查证书是不是这个 CA 签的、有没有过期；旧证书要被拒绝，需要 DDS 实现支持证书吊销列表（CRL），这一点因实现和版本而异，要自己确认。不支持时，可靠的办法是轮换 CA：用新 CA 重新签发所有 enclave，更新所有设备的 `public/` 和 enclave。这也是 CA 私钥必须安全保存的原因：丢了就做不了这些操作。
3. 同时排查攻击路径：它能拿到 `key.pem`，说明文件权限或设备访问控制有问题（🔍 容器里以 root 创建的密钥是 `666`）。
