# 答案：访问控制策略的设计

> 对应：[access-control-design.md](../design/access-control-design.md)

## 第 1 题

下面的结论都在容器里按这份 policy 实测过（`sec_probe`，enclave `/robot/misc`）🔍。

**（a）不能。** 两个 profile 在同一个 enclave 里，权限取并集，而且 **DENY 优先于 ALLOW，跨 profile 也一样** 📐。`logger` 的 profile 里拒绝发布 `cmd_vel`，这条拒绝规则作用于整个 enclave，`teleop` 的 `cmd_*` 允许被它压过 🔍。生成的 `permissions.xml` 里 `rt/cmd_vel` 出现在 `<deny_rule>` 中，排在所有 `<allow_rule>` 前面 🔍。

**（b）**

- `cmd_vel_raw`：**能**。`rt/cmd_*` 匹配 `rt/cmd_vel_raw`，而且它不在拒绝列表里 🔍。
- `cmd/arm`：**不能**。`cmd_*` 要求以 `cmd_` 开头，`cmd/arm` 的第四个字符是 `/` 不是 `_` 🔍。
- 但要小心：`*` 本身会跨过 `/`，所以 `cmd_x/y`、`cmd_vel/_action/...` 这类名字都会被匹配 🔍（练习第 2 部分的 `fib*` 实验）。

**（c）** `logger` 的 `<topic>*</topic>` 翻译成 `rt/*` 🔍，能订阅**所有话题**，包括动作内部的 `rt/<action>/_action/feedback`、`status`，但不包括服务（服务在 DDS 层是 `rq/`、`rr/` 开头）💡。问题有三个：

1. **违反最小权限** 📐：一个日志节点不需要订阅所有话题。
2. **权限会"漏"给 `teleop`**：并集之后 `teleop` 也能订阅所有话题，实测 `teleop` 订阅 `cmd_vel` 成功 🔍。
3. **拒绝规则写在 `logger` 里，却约束了 `teleop`**：读 policy 的人很难看出来 💡。

改法：`logger` 放进单独的进程和 enclave，列出它真正需要的话题；`/cmd_vel` 的 DENY 放在一个注释清楚的"公共兜底"文件里，用 XInclude 引用 💡。

## 第 2 题

**（a）两份都起不来** 🔍。

- 加了命名空间后，节点全名变成 `/left/controller_server`。它的参数服务在 DDS 层叫 `rq/left/controller_server/get_parametersRequest` 等等 📐。
- 原来的 profile 是 `ns="/" node="controller_server"`，翻译出来的是 `rq/controller_server/...`，名字对不上 🔍。
- 失败发生在**节点构造时**：rclcpp 创建参数服务的那一步，`Enforce` 下报 `... topic not found in allow rule`，接着 `could not create service`，进程退出。第 7 批练习里 `-r __node:=spy` 的失败就是这个过程 🔍。
- 就算参数服务放行了，相对名字的话题（如 `cmd_vel` 变成 `/left/cmd_vel`）也会在创建发布者时被拒 💡。

**（b）思路：让 policy 和 launch 用同一份"参数"生成，而不是手改 XML** 💡（设计文档"组合性"一节建议节点提供清单，Rolling 里还没有这种格式 ⏳）。

1. **每个节点写一份"名义接口"清单**，只写相对名字，例如 `publish: [cmd_vel]`、`subscribe: [odom, plan]`。可以就放在包里，当作设计文档说的 manifest。
2. **用模板生成 policy**：一个小脚本（或 XSLT 参数）读清单和命名空间，生成 `<profile ns="/left" node="controller_server">`。公共权限用 XInclude 引用 📐。
3. **launch 里每份控制器用自己的 enclave**，例如 `/arm/left`、`/arm/right`，和命名空间一起传进去。两个手臂互不影响，一份被攻破也碰不到另一份的话题 💡。
4. **CI 里验证**：起一遍 launch，用 `ros2 security generate_policy` 从运行中的图导出 policy，和生成的 policy 做 diff 🔍（命令存在，见[安全的设计](../design/security-design.md)对照表）。

这样"换命名空间"只是改 launch 的一个参数，policy 跟着重新生成、重新签名。
