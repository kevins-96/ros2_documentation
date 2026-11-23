# 答案：名字与重映射的设计

> 对应：[remapping-design.md](../design/remapping-design.md)

## 第 1 题

**（a）**
```bash
ros2 run lidar_pkg dual_lidar --ros-args \
  -r front_lidar:scan:=front/scan \
  -r rear_lidar:scan:=rear/scan
```
节点前缀让规则只作用于一个节点 📐 🔍。替换侧是相对名，会按节点的命名空间 `/robot1` 展开成 `/robot1/front/scan` 🔍。写绝对名 `/robot1/front/scan` 也可以，只是换命名空间部署时要跟着改。

**（b）** 节点起不来。rcl 解析到 `**` 时报 `Wildcard '**' is not implemented`，`rclcpp::init` 失败，进程直接退出 🔍。通配符和反向引用是设计文档提出的语法，Rolling 还没实现 ⏳。

**（c）** 不带前缀的规则作用于进程里的**所有节点** 📐 🔍。两个节点都改名为 `/robot1/lidar`，出现同名节点：rosout 会警告 `Publisher already registered for node name` 🔍。两个节点的参数服务也会同名，`ros2 param` 分不清该找哪一个 💡。`scan` 仍是两个节点共用的 `/robot1/scan`，因为节点名不影响相对话题名，只影响 `~/` 开头的私有名。💡

## 第 2 题

**（a）** `-r rostopic://map:=map_stream`。只有话题被改名，服务仍是 `/map` 🔍。不加 URL 前缀的话，`-r map:=map_stream` 会把话题和服务一起改掉 📐。

**（b）** 得到的是"原来的命名空间 + `global_planner`"，`/robot2` 没生效 🔍。规则按类别依次应用：先应用 `planner:__node:=global_planner`，节点改名为 `global_planner`；再看命名空间规则时，`planner:` 前缀已经匹配不上这个节点了 📐。正确的写法是 `-r planner:__node:=global_planner -r global_planner:__ns:=/robot2`，或者让命名空间规则不带前缀。

设计文档没有解释为什么这样排序。下面是推导 💡：节点名和命名空间决定了 `~/` 私有名和相对名展开成什么全名，而其他重映射规则都是**按全名匹配**的 📐。所以必须先确定节点名和命名空间，才能把名字展开成全名，再拿去和其他规则比较；否则同一条规则在不同顺序下会匹配到不同的名字。
