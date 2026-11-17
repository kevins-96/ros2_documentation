# 答案：动作

> 先在[卡片](../cards/communication/action.md)里自己写下答案再来对照。

## 题 1：两种策略下的结果码

| 策略 | 旧目标 | 新目标 |
|---|---|---|
| 拒绝 | 正常执行到结束：SUCCEEDED（或执行失败时 ABORTED） | 被拒绝：`goal_response_callback` 收到空句柄，没有结果码 |
| 抢占 | **ABORTED** | 被接受，结束时 SUCCEEDED / ABORTED |

🔍 [action_lab](../labs/action-preempt.md) 实测与此一致。

**区分"被抢占"和"真的失败"：** 两者结果码都是 ABORTED，结果码本身不够。💡 做法：
- 客户端自己知道刚刚发了新目标，旧目标的 ABORTED 可以忽略；
- 或者在 `.action` 的结果里加一个原因字段（例如 `uint8 reason`，常量 `PREEMPTED`、`PLANNING_FAILED`）。
- 📄 文档区分了 cancel（客户端发起）和 abort（服务端发起），但没有为"抢占"单独定义结果码。

## 题 2：在 handle_accepted 里直接执行

1. `handle_accepted` 是执行器调用的回调；直接在里面跑完整个导航过程，单线程执行器的唯一线程就一直被占着。📄 文档：这些回调必须很快返回，否则会饿死执行器。
2. 客户端的取消请求是通过 Cancel **服务**送达的，处理它需要执行器调用 `handle_cancel`。线程被占着，取消请求得不到处理。
3. 结果：取消无效，机器人一直开到目标（或执行结束）才停；期间这个节点的反馈之外的其他回调也都停了。💡 反馈仍能发出去，因为 `publish_feedback` 是在执行线程里直接调用的。
4. 修复：执行放到工作线程（卡片代码），或改成定时器分步推进。
