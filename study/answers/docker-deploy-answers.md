# 答案：Docker 部署

> 先在[卡片](../cards/middleware/docker-deploy.md)里自己写下答案再来对照。

## 题 1：三个 host 网络容器，`perception` 收不到点云

**原因：** 三个容器都用宿主机网络，IP 一样，Fast DDS 判断它们"在同一台机器上"，优先用共享内存传输。但每个容器默认有自己的 IPC 命名空间和 `/dev/shm`，`driver` 写进共享内存段的数据，`perception` 看不见，于是收不到，而且没有任何报错（🔍 实测 0 条）。`ros2 topic list` 正常，是因为发现走的是 UDP，发现本身成功了（`mw_probe` 会显示 `publishers in graph=1`）。

**两种修法：**

1. 所有容器都加 `ipc: host`（或者共享同一个 IPC 命名空间，如 `ipc: "container:driver"`），让 `/dev/shm` 共享，共享内存传输能用了（🔍 实测 39 条）。注意必须**所有**相关容器都改，只改一边没用。
2. 所有容器都设 `FASTDDS_BUILTIN_TRANSPORTS=UDPv4`，不用共享内存（🔍 实测 39 条）。

（第三种：去掉 `network_mode: host` 改用 bridge 网络，也能通，但宿主机上的 RViz 要能看到话题就得另外处理端口和发现，不符合这个场景的初衷。）

**这个场景选哪种：** 选 `ipc: host`。30 Hz × 2 MB = 60 MB/s 的点云，走共享内存基本只是内存拷贝；改成 UDPv4 后同机也要走内核网络栈，每帧拆成大量分片，CPU 和延迟都明显上去（💡 未在本次实测中量化）。代价是容器之间的 IPC 隔离变弱，对机器人内部的可信容器通常可以接受。

## 题 2：出厂机器人开启 SROS2 的密钥文件布局

| 位置 | 应该有 | 不应该有 |
|---|---|---|
| 组织内部（离线的签发机器 + 备份） | 完整密钥库：`private/`（CA 私钥）、`public/`、所有 `enclaves/`、policy 源文件 | — |
| 设备上（运行时挂载或首次配置时写入） | `public/`、`enclaves/governance.p7s`（和 `.xml`）、**这台设备上进程需要的 enclave 目录** | `private/`、其他设备或别的应用的 enclave |
| 镜像里 | 最好**什么都没有**：镜像只放程序，密钥在部署时挂载进来（每台机器人的身份不同，镜像却是同一份） | 尤其不能有 `private/` |

📄 依据：Deployment Guidelines 的表格，`private/` 只放组织内部，`public/` 和 `enclaves/` 随设备；`private/` 丢失就无法再修改权限或添加节点。

**防止私钥泄露的做法（至少两条）：**

1. `.dockerignore` 里排除整个密钥库目录，镜像构建时根本拿不到；部署用 volume 只挂载需要的文件（像文档 compose 示例那样逐个挂载 `governance.p7s`、自己的 enclave、`public/`）。
2. CA 私钥放在离线机器或硬件安全模块（📄 文档提到可以用 PKCS#11 URI 让 enclave 私钥存进 HSM），签发流程在组织内完成，设备上只有签好的结果。
3. 设备上 `enclaves/` 只读（📄），`key.pem` 权限 `600`、属主是运行节点的用户（🔍 容器里以 root 创建时是 `666`）。
4. 每台设备、每个应用单独的 enclave，泄露一个只需要吊销这一个，不影响其他设备。
5. 运行时 `ROS_SECURITY_STRATEGY=Enforce`，密钥缺失或签名无效时节点不启动，而不是悄悄降级成明文。
