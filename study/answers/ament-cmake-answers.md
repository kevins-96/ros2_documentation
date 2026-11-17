# 答案：ament_cmake

> 先在[卡片](../cards/engineering/ament-cmake.md)里自己写下答案再来对照。

## 题 1：`geom_utils` 下游用不了

🔍 2026-11-17 在 Rolling 镜像里按题目原样建了 `geom_utils` 和 `local_planner` 两个包，逐步修复，每一步的报错如下：

| 问题 | 下游（或上游）看到的报错 |
|---|---|
| ① 没有导出集：`install(TARGETS)` 没写 `EXPORT`，也没有 `ament_export_targets` | 上游编过；下游 `Target "planner" links to: geom_utils::geom_utils but the target was not found` |
| ② 补了导出，但 `target_include_directories(geom_utils PUBLIC include)` 是源码目录里的相对路径 | **上游**报 `INTERFACE_INCLUDE_DIRECTORIES property contains path: ".../src/geom_utils/include" which is prefixed in the source directory`。导出给下游的目标不能指向源码目录 |
| ③ 头文件没 `install` | 下游报 `includes non-existent path ".../install/geom_utils/include/geom_utils"`（练习里删掉头文件安装实测过） |
| ④ 公开头文件里 `#include <Eigen/Core>`，库也链接了 `Eigen3::Eigen`，但没有 `ament_export_dependencies(Eigen3)` | 下游 `find_package(geom_utils)` 时报 `The link interface of target "geom_utils::geom_utils" contains: Eigen3::Eigen` |

改正后（实测两个包都编过，`planner` 能运行）：

```cmake
target_include_directories(geom_utils PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:include/${PROJECT_NAME}>")
target_link_libraries(geom_utils Eigen3::Eigen)
install(DIRECTORY include/ DESTINATION include/${PROJECT_NAME})
install(TARGETS geom_utils EXPORT export_geom_utils
  ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
ament_export_targets(export_geom_utils HAS_LIBRARY_TARGET)
ament_export_dependencies(Eigen3)
ament_package()
```

另外两处不影响编译但应改：
- 💡 少了 `cmake_minimum_required`（CMake 会警告），📄 也没有 `-Wall -Wextra -Wpedantic`；
- 📄 `install(TARGETS)` 只写了 `LIBRARY DESTINATION`，建议把 `ARCHIVE`（静态库）和 `RUNTIME`（Windows DLL）也写上。🔍 题目里的库没写 `SHARED`，实测装出来的是静态库 `libgeom_utils.a`（它只依赖 Eigen，见卡片常见坑 8）；💡 静态库能装上是因为 CMake 对没写目标位置的类型会用默认的 `lib`，不要依赖这个。

💡 规律：**上游自己编过，不说明导出对了**。①③④ 都只在下游暴露。公共库包最好带一个下游测试包，在 CI 里一起编。

## 题 2：`ament_target_dependencies` 迁移

**(a)** 🔍 Rolling 报 `Unknown CMake command "ament_target_dependencies"`。📄 Kilted 起这个宏被弃用（会打印弃用警告），🔍 在 Rolling 里已经删除：`ament_cmake_target_dependencies` 包目录还在，但没有宏文件了。

**(b)**

```cmake
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
target_link_libraries(controller
  rclcpp::rclcpp
  ${geometry_msgs_TARGETS})
```

**(c)** 🔍 Rolling 的消息包里有一个 `rosidl_cmake_aggregate_target-extras.cmake`，生成了聚合目标 `geometry_msgs::geometry_msgs`，它链接 `${geometry_msgs_TARGETS}` 里的全部类型支持目标（文件注释说这是为了"用一个现代 CMake 目标名代替 `${geometry_msgs_TARGETS}`"）。💡 这个聚合目标是新加的，Jazzy 的消息包里不一定有；要同时支持 Jazzy 和 Rolling，用 `${geometry_msgs_TARGETS}` 最稳妥（`ros2 pkg create` 生成的模板也这么写：`${rclcpp_TARGETS}`）。请在 Jazzy 环境里 `grep -r aggregate /opt/ros/jazzy/share/geometry_msgs/cmake/` 自己确认。
