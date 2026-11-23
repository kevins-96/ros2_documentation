# 练习：安全与工程化的设计在 Rolling 里还成立吗？

> 适用：ROS 2 **Rolling**（默认 RMW `rmw_fastrtps_cpp`，sros2 0.17.1，colcon-core 0.21.3）。对应设计笔记：[安全的设计](../design/security-design.md)、[访问控制策略的设计](../design/access-control-design.md)、[威胁模型](../design/threat-model-design.md)、[构建系统的设计](../design/build-system-design.md)、[发现与协商的设计](../design/discovery-negotiation-design.md)
> 代码：[design_lab/](design_lab/)（本批新增 `sec_probe`、`graph_probe`、[config/sec_design_policy.xml](design_lab/config/sec_design_policy.xml)、[config/aarch64-toolchain.cmake](design_lab/config/aarch64-toolchain.cmake)）
> 用时：约 60 分钟。第 7 部分要装交叉编译器（约 100 MB）

做法和前几批的练习（[第 8 批](design-claims-check.md)、[第 9 批](config-design-check.md)、[第 10 批](perf-design-check.md)）一样：先填预测，再运行，最后展开参考结果。**预测错了的地方，就是你对设计理解有偏差的地方。**

## 0. 编译和准备

```bash
ln -s <你的仓库路径>/study/labs/* ~/study_ws/src/
cd ~/study_ws && source /opt/ros/rolling/setup.bash
colcon build --packages-up-to design_lab && source install/setup.bash
```

或者用官方镜像：`docker run -it --rm -v <你的仓库路径>/study/labs:/ws/src/labs ros:rolling-ros-base`，在容器里执行 `cd /ws && colcon build --packages-up-to design_lab`。

`sec_probe` 在**同一个进程**里按参数建若干个节点，逐个尝试创建发布者或订阅者，每一项单独 `try/catch`，打印 `OK` 或 `FAIL`：

```text
sec_probe <节点名>:<话题>[:pub|sub] ...        例：sec_probe a:ta b:tb a:tb:sub
sec_probe --second-context=/某个enclave ...    另建一个 context，用另一个 enclave
```

第 1–3 部分共用一个密钥库：

```bash
mkdir -p ~/sec11 && cd ~/sec11
ros2 security create_keystore ks
for e in /design/multi /design/deny /design/glob /design/plain; do ros2 security create_enclave ks $e; done
P=$(ros2 pkg prefix design_lab)/share/design_lab/config/sec_design_policy.xml
for e in /design/multi /design/deny /design/glob; do ros2 security create_permission ks $e $P; done
sec(){ export ROS_SECURITY_KEYSTORE=~/sec11/ks ROS_SECURITY_ENABLE=true ROS_SECURITY_STRATEGY=Enforce; }
```

`/design/plain` 用 `create_enclave` 生成的默认权限（什么都允许），另外三个用 [sec_design_policy.xml](design_lab/config/sec_design_policy.xml) 里的策略。Fast DDS 的安全错误是彩色的多行输出，看结果时加 `2>&1 | grep -E "^(node|pub|sub|ctx2|recv)|Found"`。

## 1. enclave 怎么选（[安全的设计](../design/security-design.md)）

```bash
sec
ROS_SECURITY_ENCLAVE_OVERRIDE=/design/plain ros2 run design_lab sec_probe a:other --ros-args -e /design/glob   # (a)
ros2 run design_lab sec_probe a:other                                                                       # (b) 不写 -e
ROS_SECURITY_STRATEGY= ros2 run design_lab sec_probe a:other                                                # (c) 不写 -e，宽松模式
ros2 run design_lab sec_probe a:other --ros-args -e design/plain                                            # (d) 相对路径
ros2 run design_lab sec_probe a:other --ros-args -r __ns:=/robot7 -e /design/plain                          # (e) 命名空间和 enclave 无关
ros2 run design_lab sec_probe --second-context=/design/plain a:other --ros-args -e /design/glob             # (f) 一个进程两个 enclave
grep -rn enclave $(python3 -c "import launch_ros,os;print(os.path.dirname(launch_ros.__file__))") | head    # (g)
```

| # | 问题 | 预测 | 实测 |
|---|---|---|---|
| (a) | 覆盖变量和 `-e` 同时设，用哪个？`a` 能发 `other` 吗 | | |
| (b) | 不写 `-e`，rcl 去哪里找安全文件？结果如何 | | |
| (c) | 同上，但不是 `Enforce` | | |
| (d) | enclave 能写相对路径吗 | | |
| (e) | 节点在 `/robot7` 命名空间，用 `/design/plain` 行不行 | | |
| (f) | 一个进程里两个 context 各用一个 enclave，行不行 | | |
| (g) | launch_ros 支持推送 enclave 吗 | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

2026-11-23 在 `ros:rolling-ros-base` 容器里实测：

- (a) 用覆盖变量：`Found security directory: .../enclaves/design/plain`，`a` 发 `other` 成功。如果用的是 `-e /design/glob`，发 `other` 会被拒。📐 设计文档写明覆盖变量优先。
- (b) 根 enclave：`Found security directory: .../ks/enclaves`。根目录下没有证书，节点创建失败：`couldn't find all security files!`。📐 "没写路径的节点默认用根 enclave"。
- (c) 同样找的是根目录，但宽松模式下**照常启动**，以不安全模式运行。
- (d) 不行，`rclcpp::init` 直接抛异常退出：`Enclave name is not valid: 'context name must be absolute, it must lead with a '/''`。设计文档设想的"launch 推送相对路径"在 rcl 这一层就不支持。
- (e) 可以，正常运行。📐 enclave 路径和命名空间脱钩。
- (f) 可以：两行 `Found security directory`，一行 `.../design/glob`、一行 `.../design/plain`；第二个 context 里的 `z` 发 `other` 成功，第一个 context 里的 `a` 发 `other` 失败（`/design/glob` 只允许 `fib*`）。设计文档把"一个进程该不该有多个 enclave"列为顾虑，Rolling 实际是允许的。
- (g) 没有输出：launch_ros 里没有任何 enclave 相关的代码，`PushROSNamespace` 只收 `namespace`。⏳

</details>

## 2. 访问控制策略：合并、DENY、通配符（[访问控制策略的设计](../design/access-control-design.md)）

先看生成的权限文件：

```bash
grep -c "<grant" ks/enclaves/design/multi/permissions.xml
grep -n "rt/t[ab]" ks/enclaves/design/multi/permissions.xml
grep -n -E "<grant|deny_rule|allow_rule|rt/ta|<default>" ks/enclaves/design/deny/permissions.xml
grep -n "fib\|ros_discovery_info" ks/enclaves/design/glob/permissions.xml
```

再运行：

```bash
sec
ros2 run design_lab sec_probe a:ta a:tb b:tb b:ta c:ta --ros-args -e /design/multi       # (A)
ros2 run design_lab sec_probe a:ta b:tb --ros-args -e /design/deny                       # (B)
ros2 run design_lab sec_probe a:fib a:fib_x a:fib/_action/status a:fib/_action/feedback \
  a:fib/deep/x a:/fib a:other --ros-args -e /design/glob                                 # (C)
```

| # | 问题 | 预测 | 实测 |
|---|---|---|---|
| 1 | `/design/multi` 有两个 profile，生成几个 `<grant>` | | |
| 2 | (A) profile a 只允许发 `ta`，节点 a 发 `tb` 行不行？b 发 `ta` 呢 | | |
| 3 | (A) 没有 profile 的节点 c 会怎样，失败在哪一步 | | |
| 4 | (B) profile a 允许发 `ta`、profile b 拒绝发 `ta`，a 能发 `ta` 吗 | | |
| 5 | (B) 生成的文件里 DENY 和 ALLOW 谁在前 | | |
| 6 | (C) `fib*` 会匹配到哪些 | | |
| 7 | 设计文档没提的那条额外规则是什么 | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

1. 1 个。两个 profile 合并成一个 grant，`<publish>` 列表里 `rt/ta`、`rt/tb` 挨在一起，参数服务 `rq/a/...`、`rq/b/...` 也在一起。
2. **都行**：`pub a tb OK`、`pub b ta OK`。DDS 只认参与者，同一个 enclave 里的 profile 是并集。
3. **节点构造时就失败**：`node c FAIL: could not create service: create_service() failed to create request DataReader`，Fast DDS 报 `rq/c/get_parametersRequest topic not found in allow rule.`。节点名在这里还是起作用的：参数服务的名字是按 profile 的 `node` 展开的。
4. **不能**：`rt/ta topic denied by deny rule.`。DENY 优先，跨 profile 也一样。（顺带：b 发 `tb` 也失败，因为没有任何 profile 允许 `tb`。）
5. 先 `<deny_rule>`（第 9 行附近），再 `<allow_rule>`，最后 `<default>DENY</default>`。📐 模板故意把拒绝规则排在前面，因为 DDS 按顺序匹配。对象按字母排序。
6. 除了 `other`，**全部 OK**：`fib`、`fib_x`、`fib/_action/status`、`fib/_action/feedback`、`fib/deep/x`、`/fib`（绝对名 `/fib` 和相对名 `fib` 在 DDS 里都是 `rt/fib`）。`*` 会跨过 `/`，于是"允许 `fib*` 话题"顺带允许了动作 `fib` 的内部话题。📐 这正是设计文档"不鼓励通配符"的原因。
7. 每个 grant 最后都有一条 `<allow_rule>`，允许发布和订阅 `ros_discovery_info`（没有 `rt/` 前缀）。这是 rmw 自己用的 DDS 话题，见[发现与协商的设计](../design/discovery-negotiation-design.md)。

</details>

## 3. 安全日志插件

设计文档说 ROS 2 不依赖日志插件。试试它在 Fast DDS 上能不能用：

```bash
sec
ROS_SECURITY_LOG_FILE=/tmp/seclog.txt ROS_SECURITY_LOG_VERBOSITY=WARNING \
  ros2 run design_lab sec_probe a:ta --ros-args -e /design/deny          # (a) 注意取值
ROS_SECURITY_LOG_FILE=/tmp/seclog.txt ROS_SECURITY_LOG_VERBOSITY=DEBUG \
  ros2 run design_lab sec_probe a:ta --ros-args -e /design/deny          # (b)
cat /tmp/seclog.txt
```

| # | 问题 | 预测 | 实测 |
|---|---|---|---|
| (a) | `WARNING` 是合法取值吗 | | |
| (b) | 日志文件里记了什么，什么级别 | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

- (a) 不合法，节点创建失败：`ROS_SECURITY_LOG_VERBOSITY is not valid: WARNING is not a supported verbosity (use FATAL, ERROR, WARN, INFO, or DEBUG)`。取值和 ROS 日志级别的名字一致。
- (b) 一行：`[EMERGENCY] <参与者 GUID> ... Permissions::check_create_datawriter : rt/ta topic denied by deny rule.`。被拒绝的访问记成 EMERGENCY 级别。这三个变量（`ROS_SECURITY_LOG_FILE`、`ROS_SECURITY_LOG_PUBLISH`、`ROS_SECURITY_LOG_VERBOSITY`）在 rmw_fastrtps 里实现，本仓库文档没有提到。

</details>

## 4. 为什么不用单一 CMake 上下文（[构建系统的设计](../design/build-system-design.md)）

建两个最小的 ament_cmake 包，都定义一个叫 `util` 的库：

```bash
mkdir -p ~/bw/src && cd ~/bw/src
for p in pkg_a pkg_b; do
  ros2 pkg create --build-type ament_cmake $p >/dev/null
  echo "int ${p}_f(){return 1;}" > $p/src/util.cpp
  sed -i 's#^ament_package()#add_library(util SHARED src/util.cpp)\ninstall(TARGETS util DESTINATION lib/${PROJECT_NAME})\nament_package()#' $p/CMakeLists.txt
done
cd ~/bw && colcon build                                   # (a)

mkdir -p ~/bw_single && cd ~/bw_single                    # (b) 模拟 catkin_make
printf 'cmake_minimum_required(VERSION 3.20)\nproject(all_in_one)\nadd_subdirectory(%s/bw/src/pkg_a pkg_a)\nadd_subdirectory(%s/bw/src/pkg_b pkg_b)\n' $HOME $HOME > CMakeLists.txt
cmake -S . -B build 2>&1 | grep -A2 "CMake Error"
```

| # | 问题 | 预测 | 实测 |
|---|---|---|---|
| (a) | 两个包都有目标 `util`，colcon 能编吗 | | |
| (b) | 放进一个 CMake 上下文呢 | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

- (a) 能，`2 packages finished`。每个包单独配置、单独构建。
- (b) 不能：`add_library cannot create target "util" because another target with the same name already exists.`。📐 这就是 ament 放弃 `catkin_make` 式构建的理由之一。

</details>

## 5. 环境变量和 ament 资源索引

再加一个没有 `package.xml` 的 CMake 工程和一个只有 `setup.py` 的 Python 包：

```bash
cd ~/bw/src
mkdir -p plain_lib && printf 'cmake_minimum_required(VERSION 3.20)\nproject(plain_lib)\nadd_library(plain SHARED p.cpp)\ninstall(TARGETS plain)\n' > plain_lib/CMakeLists.txt
echo "int p(){return 2;}" > plain_lib/p.cpp
mkdir -p py_pkg/py_pkg && touch py_pkg/py_pkg/__init__.py
printf 'from setuptools import setup\nsetup(name="py_pkg", version="0.0.1", packages=["py_pkg"])\n' > py_pkg/setup.py
cd ~/bw && colcon list && colcon build

# 新终端
env -i HOME=$HOME bash -c 'source /opt/ros/rolling/setup.bash; echo "AMENT=$AMENT_PREFIX_PATH"; echo "CMAKE=${CMAKE_PREFIX_PATH:-<unset>}"'
source /opt/ros/rolling/setup.bash && source ~/bw/install/setup.bash
ros2 pkg prefix pkg_a; ros2 pkg prefix plain_lib
mv install/pkg_a/share/ament_index/resource_index/packages/pkg_a /tmp/marker
ros2 pkg prefix pkg_a; ls install/pkg_a/lib/pkg_a
mv /tmp/marker install/pkg_a/share/ament_index/resource_index/packages/pkg_a
colcon extension-points | wc -l
```

| # | 问题 | 预测 | 实测 |
|---|---|---|---|
| 1 | `colcon list` 怎么识别两个没有 `package.xml` 的包 | | |
| 2 | 只 source 了 `/opt/ros/rolling`，`CMAKE_PREFIX_PATH` 里有什么？那 colcon 构建时 CMake 怎么找到 rclcpp | | |
| 3 | `ros2 pkg prefix plain_lib` 能找到吗 | | |
| 4 | 删掉 ament 索引里的标记文件之后呢？标记文件里写了什么 | | |
| 5 | colcon 有多少个扩展点 | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

1. `plain_lib (cmake)`、`py_pkg (python)`，和 `pkg_a (ros.ament_cmake)` 一起都能构建。📐 统一构建工具的要求之一就是"能构建非 ROS 的包"。
2. `CMAKE_PREFIX_PATH` **没设**，只有 `AMENT_PREFIX_PATH=/opt/ros/rolling`。colcon-ros 在调用 CMake 前把 `AMENT_PREFIX_PATH` 加进 `CMAKE_PREFIX_PATH`（`colcon_ros/task/__init__.py`）。📐 设计文档："构建时 CMake 专用的变量可以从通用的 ament 变量推出"。顺带：不用 colcon、手动 `cmake` 也能找到 rclcpp，靠的是 CMake 会从 `PATH` 里的 `/opt/ros/rolling/bin` 推出前缀；把 `PATH` 改成 `/usr/bin:/bin` 就找不到了。
3. 找不到：`Package not found`。它只有 colcon 自己的索引 `share/colcon-core/packages/plain_lib`，没有 ament 索引的标记。
4. `Package not found`，而 `libutil.so` 还在。标记文件是**空文件**（0 字节），存在就表示"这里有这个包"。pluginlib、`ros2 pkg`、rosidl 的类型支持查找都走这个索引。
5. 21 个。覆盖设计文档列的 9 类：verb、argument_parser、package_discovery、package_identification、task.build/test、executor、event_handler、shell、argcomplete。

</details>

## 6. symlink install、DESTDIR、mixin

```bash
cd ~/bw && rm -rf build install
colcon build --symlink-install --packages-select pkg_a
ls -l install/pkg_a/lib/pkg_a/ install/pkg_a/share/pkg_a/package.xml
sed -n '/^function(install/,/^endfunction/p' /opt/ros/rolling/share/ament_cmake_core/cmake/symlink_install/install.cmake

rm -rf build install && DESTDIR=/tmp/stage colcon build --packages-select plain_lib
find /tmp/stage -name "*.so"; ls install/plain_lib/lib

colcon mixin list; colcon mixin show | grep -E "^- " | tr '\n' ' '
```

| # | 问题 | 预测 | 实测 |
|---|---|---|---|
| 1 | symlink install 装出来的是什么？怎么实现的 | | |
| 2 | 设了 `DESTDIR`，文件装到哪 | | |
| 3 | 默认 mixin 里有交叉编译的吗 | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

1. `libutil.so -> .../build/pkg_a/libutil.so`、`package.xml -> .../src/pkg_a/package.xml`，都是符号链接。实现就是 ament_cmake **重新定义了 CMake 的 `install()` 函数**：`DIRECTORY`、`FILES`、`PROGRAMS`、`TARGETS` 四种签名转给 `ament_cmake_symlink_install_*`，其他回落到原来的 `_install()`。📐 和设计文档一致。
2. `/tmp/stage/<你的 install 前缀>/plain_lib/lib/libplain.so`，`install/` 下没有。打包、做镜像时有用。
3. 没有。默认仓库在 `colcon-mixin-repository` 的 `index.yaml`，列出 27 条（`release`、`debug`、`asan-gcc`、`tsan`、`ccache`、`ninja`、`build-testing-off`……），没有设计文档设想的 `cc-<平台>` 这类 mixin。⏳

</details>

## 7. 交叉编译为什么需要 sysroot

装交叉编译器，用本包的 [aarch64-toolchain.cmake](design_lab/config/aarch64-toolchain.cmake)：

```bash
sudo apt install g++-aarch64-linux-gnu
TC=$(ros2 pkg prefix design_lab)/share/design_lab/config/aarch64-toolchain.cmake
cd ~/bw/src && ros2 pkg create --build-type ament_cmake --dependencies rclcpp --node-name hi cc_hi && cd ~/bw
rm -rf build install log
colcon build --packages-select plain_lib --cmake-args -DCMAKE_TOOLCHAIN_FILE=$TC                  # (a)
readelf -h install/plain_lib/lib/libplain.so | grep Machine
colcon build --packages-select cc_hi --cmake-args -DCMAKE_TOOLCHAIN_FILE=$TC                      # (b)
grep -E "Found rclcpp|CMake Error|Could not find" log/latest_build/cc_hi/stdout_stderr.log | head
rm -rf build/cc_hi && mkdir -p /tmp/fake_sysroot
colcon build --packages-select cc_hi --cmake-args -DCMAKE_TOOLCHAIN_FILE=$TC -DCMAKE_SYSROOT=/tmp/fake_sysroot   # (c)
```

| # | 问题 | 预测 | 实测 |
|---|---|---|---|
| (a) | 没有依赖的库，只给工具链文件能交叉编译吗 | | |
| (b) | 依赖 rclcpp 的包，没有 sysroot 时 CMake 找到了哪个 rclcpp？在哪失败 | | |
| (c) | 给一个空 sysroot 并限制查找范围呢 | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

- (a) 能：`Machine: AArch64`。📄 "没有外部依赖的软件交叉编译相对简单，只要换成交叉工具链"。
- (b) `Found rclcpp: 33.2.0 (/opt/ros/rolling/share/rclcpp/cmake)`：**找到了宿主机 x86 的那份**，然后在 `builtin_interfaces` 依赖的 `fastcdr` 上失败（`Could not find a package configuration file provided by "fastcdr"`），因为 `fastcdr` 的配置在 `lib/x86_64-linux-gnu/cmake/` 下，而交叉编译时 CMake 找的是 aarch64 的多架构目录。失败位置是"碰巧"的。
- (c) 第一个 `find_package(ament_cmake)` 就失败。错得早、错在明处：缺的就是 sysroot。

💡 完整的交叉编译还需要一个装好 aarch64 版 ROS 2 和系统依赖的 sysroot（设计文档的做法：Docker + QEMU 运行目标平台镜像，`rosdep install` 后导出文件系统）。原来的 `cross_compile` 工具已经弃用，本仓库文档推荐的替代方案是 `docker buildx` 构建多平台镜像。

</details>

## 8. 图事件和存活性（[发现与协商的设计](../design/discovery-negotiation-design.md)）

`graph_probe watch <话题> [秒]` 等图变化事件，话题的发布者数变化时打印时刻和端点信息；`graph_probe pub <话题> <延时>` 延时后创建一个发布者，2 秒后退出。

```bash
B=$(ros2 pkg prefix design_lab)/lib/design_lab/graph_probe
# (a) 正常退出
$B watch /gp 10 & sleep 1.5; $B pub /gp 1.0; wait
# (b) kill -9
$B watch /gp2 30 & sleep 1.5; $B pub /gp2 0.5 & P=$!; sleep 1.5; kill -9 $P; echo "killed t=$(date +%s%3N)"; wait
```

| # | 问题 | 预测 | 实测 |
|---|---|---|---|
| 1 | 从创建发布者到收到图事件要多久 | | |
| 2 | 端点信息里能看到什么 | | |
| 3 | 10 秒里一共有多少个图事件？为什么比你想的多 | | |
| 4 | 发布端正常退出，多久从图里消失？`kill -9` 呢 | | |

<details>
<summary>参考结果（填完预测再展开）</summary>

1. 约 2 ms（两个进程的 `t=` 相减）。
2. `node=/graph_pub type=std_msgs/msg/String reliability=reliable durability=transient_local depth=5`，以及 `type_hash version=1`。📐 这就是设计文档说的"节点配置 API"。
3. 约 48 个，而我们关心的只有 2 个。另一个进程起一个节点，就会因为参数服务、`rosout`、`/parameter_events` 等端点产生二十来个事件。所以醒来后一定要重新检查条件。
4. 正常退出约 2 ms；**`kill -9` 约 19.5 秒**。进程来不及注销，对端要等参与者的租期（Fast DDS 默认 20 秒）过期。📐 "意外掉线的节点发不出事件，只能靠心跳超时"。💡 安全相关的超时检测别靠图，用 DEADLINE QoS 或自己检查消息时间戳。

</details>

## 挑战（选做）

1. 按[安全的设计](../design/security-design.md)第 6 节第 1 题的思路，起一个组件容器，加载两个组件，给其中一个写只允许订阅的 profile。验证它在同一个容器里能发布另一个组件的话题；把它移到第二个容器、用另一个 enclave 后就不行了。
2. 写一个最小的 sysroot：`docker run --platform linux/arm64 ros:rolling-ros-base`（需要 `qemu-user-static`），把 `/opt/ros/rolling`、`/usr/include`、`/usr/lib/aarch64-linux-gnu` 导出到宿主机，再用第 7 部分的工具链文件交叉编译 `cc_hi`，用 `readelf` 确认架构。
3. 在 `graph_probe` 里给订阅加 `deadline(100ms)` 和 `deadline_callback`，重做第 8 部分 (b)，比较 DEADLINE 事件和图事件哪个先到。
