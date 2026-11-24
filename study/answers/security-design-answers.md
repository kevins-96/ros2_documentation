# 答案：安全的设计

> 对应：[security-design.md](../design/security-design.md)

## 第 1 题

**（a）能发布 `/cmd_vel`** 🔍 📐。

- 组件容器是一个进程、一个 context，也就是一个 DDS 参与者，只能有一个安全身份 📐。
- 所以 `/robot/nav` 这个 enclave 的权限是容器里**所有** profile 的并集：`controller_server` 的 profile 允许发 `/cmd_vel`，合并后整个参与者都能发 📐。
- DDS 的访问控制只认参与者，不认节点。`people_tracker` 在同一个参与者里创建 `/cmd_vel` 的发布者，检查的是合并后的权限，所以会通过。练习第 2 部分实测：profile a 只写了 `ta`，节点 a 发 `tb` 照样成功 🔍。
- policy 里给 `people_tracker` 单独写的 profile 只是"声明"，在同一个进程里没有隔离作用 💡。

**（b）把它移到另一个进程、用另一个 enclave** 💡：

1. 新建一个容器（或者直接 `ros2 run`），只加载 `people_tracker`，用 `-e /robot/perception` 启动。组件不用改代码，只是换个容器加载 📐（见卡片 [组件](../cards/concurrency/component.md)）。
2. `/robot/perception` 的 policy 只允许订阅 `/scan`、发布 `/people`，再加上每个节点都需要的公共权限。
3. 最好再加一条兜底：`<topics publish="DENY"><topic>/cmd_vel</topic></topics>`。DENY 优先，以后有人放宽 ALLOW 也发不了 🔍。
4. 进一步：用另一个系统用户运行这个进程，让它读不到 `/robot/nav` 的私钥 💡（见[威胁模型](../design/threat-model-design.md)）。

代价是 `/scan` 要跨进程传，不能再走进程内通信，多一次序列化 💡。

## 第 2 题

**（a）用 `/robot/debug`** 📐 🔍。设计文档写明 `ROS_SECURITY_ENCLAVE_OVERRIDE` 优先于 `--enclave`。实测 `-e /design/glob` 加上 `OVERRIDE=/design/plain`，日志是 `Found security directory: .../design/plain` 🔍。

**（b）后果：**

- **最小权限没了。** `/robot/debug` 用的是 `create_enclave` 生成的默认权限，允许所有话题、服务、动作 🔍（第 7 批）。规划器被攻破就能发任何东西。
- **身份不对。** 证书的 CN 是 `/robot/debug`，审计日志、抓包里看到的都是调试身份 💡，排查问题时会误导人。
- **行为看着正常，最难发现。** 因为权限更宽，规划器"一切正常"。只有收紧 `/robot/planner` 的权限时才会发现"怎么改了不生效" 💡。
- 如果生产机器上的密钥库里没有 `/robot/debug`，`Enforce` 下规划器反而启动失败 🔍，至少能暴露出来。

另外，`~/.bashrc` 只对交互式 shell 生效。用 systemd 启动的服务不读它 💡，所以同一台机器上"手动启动"和"开机启动"的行为会不一样，更难查。

**（c）启动前检查（示例）** 💡：

```bash
#!/usr/bin/env bash
set -euo pipefail
die(){ echo "SECURITY CHECK FAILED: $*" >&2; exit 1; }

[[ -z "${ROS_SECURITY_ENCLAVE_OVERRIDE:-}" ]] || die "ROS_SECURITY_ENCLAVE_OVERRIDE=${ROS_SECURITY_ENCLAVE_OVERRIDE} 会盖掉 -e"
[[ "${ROS_SECURITY_ENABLE:-}" == "true" ]]     || die "ROS_SECURITY_ENABLE 必须正好是 true（区分大小写）"
[[ "${ROS_SECURITY_STRATEGY:-}" == "Enforce" ]] || die "ROS_SECURITY_STRATEGY 必须是 Enforce"

E=/robot/planner
D="${ROS_SECURITY_KEYSTORE}/enclaves${E}"
for f in cert.pem key.pem permissions.p7s governance.p7s identity_ca.cert.pem permissions_ca.cert.pem; do
  [[ -r "$D/$f" ]] || die "缺少 $D/$f"
done
openssl smime -verify -in "$D/permissions.p7s" -CAfile "$D/permissions_ca.cert.pem" -out /dev/null 2>/dev/null \
  || die "permissions.p7s 签名无效"

exec ros2 run my_pkg planner --ros-args -e "$E"
```

启动之后再在监控里确认一次日志里有 `Found security directory: .../enclaves/robot/planner`（rcl 的 INFO 日志）🔍。
