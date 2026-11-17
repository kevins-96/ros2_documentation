# 答案：参数

> 先在[卡片](../cards/configuration/parameter.md)里自己写下答案再来对照。

## 题 1：三种设置方式的结果

| 情况 | 结果 | 原因 |
|---|---|---|
| (a) YAML 写 `max_speed: 2` | **节点启动即崩溃**，`InvalidParameterTypeException` | 🔍 YAML 把 `2` 解析成整数，而参数声明的是 double。类型检查发生在范围检查之前，所以根本走不到"2 在 0~2 之内"这一步。改成 `2.0` 就能启动 |
| (b) 启动时 `-p controller.kp:=-0.5` | **被接受**，kp = -0.5 | 🔍 启动覆盖值在 `declare_parameter` 时生效，而 set 回调是在那之后才注册的，拦不住。描述符里的范围限制能拦（所以 (a) 改成 `5.0` 也会崩），回调不能 |
| (c) 运行中 `ros2 param set /ctrl controller.kp -0.5` | **失败**，返回回调里写的 `reason` | 🔍 运行时修改要经过 set 回调，`successful = false` 就拒绝 |

💡 结论：只靠 set 回调做校验，会漏掉启动值。两种补法：
1. 能用描述符表达的约束（范围、只读）尽量写进描述符，它对启动值也有效。
2. 构造函数里声明完参数后，用同一个校验函数把启动值再检查一遍，不合法就抛异常或打 FATAL 退出。

## 题 2：两台机器人共用和分别设置的参数

🔍 实测用的是练习里的 `planner` 节点（`/robot1/planner`、`/robot2/planner` 同时运行），实测文件就是把下面 YAML 里的 `controller` 全部换成 `planner`、`kp` 放在 `controller:` 下。下面按题目写成 `controller`：

```yaml
/**/controller:          # 两台共用
  ros__parameters:
    kp: 1.5
/robot1/controller:
  ros__parameters:
    max_speed: 1.0
/robot2/controller:
  ros__parameters:
    max_speed: 0.5
```

结果：robot1 的 `max_speed = 1.0`、robot2 的 `max_speed = 0.5`，两台的 `kp` 都是 1.5。通配符段和精确名字段的参数会**合并**到同一个节点上。

如果把 robot1 那段的键写成 `controller:`：它只匹配**根命名空间**下的 `/controller`，和 `/robot1/controller` 不匹配。🔍 实测这种情况**不报任何错**，robot1 的 `max_speed` 是代码里的默认值（练习里是 0.5），`kp` 仍然是 1.5（来自通配符段）。

💡 这类错误很隐蔽，因为"部分参数生效"看起来像是配置文件读到了。启动后用 `ros2 param get /robot1/controller max_speed` 核对一个只在精确段里出现的值。

📄 相关规则：`*` 匹配一段，`**` 匹配零段或多段；`/**/controller` 能同时匹配 `/controller` 和 `/robot1/controller`。
