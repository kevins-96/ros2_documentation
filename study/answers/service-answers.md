# 答案：服务

> 先在[卡片](../cards/communication/service.md)里自己写下答案再来对照。

## 题 1：订阅回调里同步等服务

**会发生什么：**
1. `rclcpp::spin(node)` 是单线程执行器。📄
2. 订阅回调发出请求后在 `wait_for(5s)` 上阻塞，占住唯一的线程。
3. 响应到达后需要执行器调用 client 的响应处理来完成 future，但线程被占着，响应处理没法执行。📄（文档：Sync-Vs-Async 与 Using-callback-groups）
4. 5 秒后 `wait_for` 超时返回，回调结束；这时 future 才可能被完成，但你已经按"超时"处理了。之后每来一个 `/goal_pose` 都重复一遍。🔍 [实验 B](../labs/executor-blocking.md) 的 `same` 情形就是这个现象：服务端已回复，客户端仍超时。
5. 期间这个节点的其他回调（包括定时器）全部被卡住。

**两种修复：**
- 推荐：改用 `async_send_request(req, callback)`，在回调里处理结果，订阅回调立即返回。📄
- 必须同步时：client 放进单独的回调组（订阅留在默认组），节点改用 `MultiThreadedExecutor`。📄 判断规则见[回调组卡片](../cards/concurrency/callback-group.md)。

## 题 2：服务用 TransientLocal

📄 文档原话：服务使用 Volatile 尤其重要，否则重启的服务端可能收到过期的请求；客户端不会收到多个响应，但服务端无法避免处理过期请求带来的副作用。

导航例子（💡）：用户 10 分钟前点过"清空代价地图"，`planner` 重启后又收到这个旧请求，在机器人正依赖障碍物信息避障时把代价地图清空了。
