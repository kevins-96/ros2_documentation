# 答案：rosdep 依赖管理

> 先在[卡片](../cards/engineering/rosdep.md)里自己写下答案再来对照。

## 题 1：选依赖标签

| 依赖 | 标签 | 理由 |
|---|---|---|
| Eigen（key `eigen`，🔍 解析为 `libeigen3-dev`） | `<depend>`，至少要 `<build_depend>` + `<build_export_depend>` | 📄 公开头文件 include 了它，下游编译你的头文件时也要它。Eigen 是纯头文件库，严格说运行时不需要，所以 `<build_depend>` + `<build_export_depend>` 是最精确的写法；💡 写 `<depend>` 更省事，代价只是运行环境多装一个头文件包 |
| fmt（key `fmt`，🔍 解析为 `libfmt-dev`） | `<depend>` | 只在 `.cpp` 里用，但 fmt 通常编译成动态库，运行时要加载 `libfmt.so`，所以不是只有编译期需要。💡 如果你用的是 header-only 模式，才可以只写 `<build_depend>` |
| `robot_state_publisher` | `<exec_depend>` | 📄 launch 文件在运行时才需要它，编译时不需要 |
| gtest（`ament_cmake_gtest`） | `<test_depend>` | 📄 只有测试需要，且不要和其他标签重复 |

**Eigen 只写 `<build_depend>` 的问题：** 📄 二进制安装你的包时不会带上 Eigen。下游包 `<depend>trajectory_utils</depend>` 后编译，include 你的 `spline.hpp` 时找不到 `Eigen/Dense`。在你自己的工作空间里不会暴露（Eigen 已经装着），在干净的 CI 或别人从 apt 装你的包时才会。

## 题 2：内部 apt 包

步骤（📄 文档的做法，🔍 练习里用 `my_motor_sdk` 实测过）：

1. 写规则文件，放进仓库（例如 `rosdep/acme.yaml`），所有人共用：
   ```yaml
   acme_motor:
     ubuntu: [acme-motor-dev]
   ```
2. 每台机器加一个 sources 文件，名字前缀**大于 20**，只补充、不覆盖：
   ```bash
   echo "yaml file://$HOME/robot_ws/src/robot_repo/rosdep/acme.yaml" | \
     sudo tee /etc/ros/rosdep/sources.list.d/30-acme.list
   ```
   💡 也可以用 `https://` 指向公司内部 Git 服务器上的原始文件，这样改规则不用每台机器同步文件。
3. `rosdep update`（不加 sudo）。🔍 不 update 时 `rosdep resolve acme_motor` 报 `no rosdep rule`。
4. 前提：机器上要能 `apt install acme-motor-dev`，也就是公司的 apt 源已经配置好。rosdep 只负责"把 key 翻译成包名并调用 apt"，不负责提供 apt 源。
5. 验证：`rosdep resolve acme_motor`、`rosdep where-defined acme_motor`，再 `rosdep install ... --simulate`。

**为什么不要顺手覆盖 `libopencv-dev`：**

- 📄 文档明确警告：用二进制安装的 ROS 时覆盖已有 key "很可能造成二进制不兼容，非常难排查"。`/opt/ros/rolling` 里的 `cv_bridge`、`image_transport` 等是对着系统 OpenCV 编译的，和你自己编译的 OpenCV 版本或编译选项不同，就会出现运行时崩溃或符号错误。
- 📄 覆盖是**整条 key 替换**：你的文件只写了 `ubuntu`，其他平台的安装方式也一起丢了。
- 🔍 而且放在 `30-` 文件里根本不会生效（`20-default` 先加载，先加载的赢）；要生效必须改成 `10-` 前缀，这本身就说明你在做一件危险的事。
- 💡 真要用自己的 OpenCV，给它起一个新的 key（例如 `acme_opencv`），只让需要它的包依赖这个新 key，并在 overlay 里重编所有相关包。
