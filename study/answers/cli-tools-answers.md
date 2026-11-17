# 答案：命令行工具

> 先在[卡片](../cards/observability/cli-tools.md)里自己写下答案再来对照。

## 题 1：`ros2 topic echo` 能看到 `/map`，规划器收不到

**最可能的原因：** QoS 不兼容。🔍 `ros2 topic echo` 会按发布者的 QoS 自动选择订阅 QoS，所以它几乎总能收到；你的节点用的是代码里写死的 QoS。地图通常是 transient local + reliable 发布一次，如果规划器用默认 QoS（volatile），它启动晚于地图服务器就永远收不到那一条。另一种情况是发布方 best effort、订阅方 reliable，直接不匹配。

**确认方法：**

```bash
ros2 topic info -v /map
```

看两段：`Endpoint type: PUBLISHER` 下的 `Reliability`、`Durability`，和 `Endpoint type: SUBSCRIPTION`（你的规划器那一条）下的同名两行。规则见 [QoS](../cards/communication/qos.md)：订阅端要求不能比发布端提供的更"强"。也可以复现差异：`ros2 topic echo --once /map --qos-durability volatile` 收不到，就说明"晚加入 + volatile"是原因。

💡 再排除一种低级错误：话题名不一致（相对名加了命名空间）。`ros2 node info /你的节点` 看它实际订阅的完整话题名。

## 题 2：脚本依次发三个目标，没有订阅者时 10 秒内失败

```bash
# goals.yaml 里三个 PoseStamped 文档，用 --- 分隔
ros2 topic pub --times 3 --max-wait-time-secs 10 \
  --yaml-file goals.yaml /goal_pose geometry_msgs/msg/PoseStamped || {
  echo "没有订阅者，放弃"; exit 1; }
```

- `--times 3`：发 3 条后退出。🔍 配合 `--yaml-file`，3 条依次取文件里的 3 个文档。
- `-r`（`--rate`）：发布频率，默认 1 Hz，所以三条间隔正好 1 秒（🔍 规划器日志时间戳相差约 1.00 s），不用另写。
- `-w`：等多少个匹配的订阅者再开始发。用 `--times` 时默认就是 1，所以不用写。
- `--max-wait-time-secs 10`：等订阅者的上限。🔍 不加它，没订阅者时会无限等待；加上 `--max-wait-time-secs 3` 实测 4 秒后打印 `Timed out waiting for subscribers: Expected 1 subscriber but only got 0 subscribers`，退出码 1。

💡 "匹配"是指 QoS 兼容的订阅者，不只是"话题上有订阅者"。如果目标点话题的订阅者要求 transient local，而 `ros2 topic pub` 默认 volatile，也会一直等，需要加 `--qos-durability transient_local`。
