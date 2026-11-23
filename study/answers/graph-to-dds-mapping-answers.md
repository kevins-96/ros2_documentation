# 答案：ROS 图怎么映射到 DDS

> 对应：[graph-to-dds-mapping.md](../design/graph-to-dds-mapping.md)

## 第 1 题

**（a）完整 ROS 名字**

| 代码里写的 | 完整名字 |
|---|---|
| 发布者 `cmd_vel` | `/robot2/cmd_vel`（相对名接在命名空间后） |
| 发布者 `~/costmap` | `/robot2/local_planner/costmap`（`~` = 命名空间/节点名） |
| 服务 `clear` | `/robot2/clear` |
| 动作 `follow_path` | `/robot2/follow_path` |

**（b）DDS 名字**（规则来自设计文档，前缀和后缀形式按 🔍 抓包结果）

- `rt/robot2/cmd_vel`
- `rt/robot2/local_planner/costmap`
- 服务：`rq/robot2/clearRequest`、`rr/robot2/clearReply`
- 动作：`rt/robot2/follow_path/_action/feedback`、`rt/robot2/follow_path/_action/status`、`rq/robot2/follow_path/_action/send_goalRequest`（以及对应的 `rr/...Reply`，另有 `cancel_goal`、`get_result` 两个服务）
- 另外每个节点还自带一批参数服务：`rq/robot2/local_planner/get_parametersRequest` 等

**（c）** `ros2 topic list` 默认显示 `/robot2/cmd_vel`、`/robot2/local_planner/costmap`，以及每个节点都有的 `/parameter_events`、`/rosout`。动作的两个话题含有以 `_` 开头的段 `_action`，属于隐藏名字，要加 `--include-hidden-topics` 才显示 📐 ✅。服务本来就不在话题列表里（`ros2 service list` 看）。

## 第 2 题

**会发生什么：** `fuser` 从此收不到 `wheel_odom` 的 `/odom`，融合结果停止更新或只剩旧数据。不会报错。🔍

**为什么：** Foxy 起一个 context 只有一个 DDS 参与者，组件容器里所有节点共用它。`ignore_local_publications` 在 Fast DDS 上忽略的是"同一个参与者"发出的样本，不是"同一个节点"的。原来 `wheel_odom` 在另一个进程（另一个参与者），所以不被忽略；合进同一个容器后，它变成了"本地发布者"，被一起忽略了。实测：同进程另一个节点发 10 条，`ignore_local_publications=true` 的订阅收到 0 条；换成独立 context 后收到 10 条。🔍 设计文档预告过这一点："以后要识别发布者来自哪个节点，需要额外的逻辑"。📐

**怎么改（💡）：**

1. 最直接：别让一个节点既发又收同名话题。把修正后的输出改名，例如 `/odom/filtered`（Nav2 的 robot_localization 就是这种习惯），这样根本不需要 `ignore_local_publications`。
2. 如果必须同名：去掉 `ignore_local_publications`，在回调里按消息内容判断来源（例如 `child_frame_id` 或自定义的来源字段），丢掉自己发的。
3. 在部署检查里加一条：任何用了 `ignore_local_publications` 的订阅，组合部署前后都要测一遍。
