# 答案：pluginlib 插件

> 先在[卡片](../cards/engineering/pluginlib.md)里自己写下答案再来对照。

## 题 1：`ros2 plugin list` 里有，`controller_server` 却说不存在

**原因：** 🔍 XML 里写了 `name="my_ctrl/PurePursuit"`，pluginlib 就只用这个名字作为查找名。YAML 里写的是 `type`（`my_ctrl::PurePursuit`），`ClassLoader::isClassAvailable` 只查以查找名为键的表，所以报不存在。练习里的同一情况：`smoother_plugin:=path_smoother_plugins::MovingAverageSmoother` 报 `does not exist. Declared types are  moving_average ...`。

`ros2 plugin list` 里"有它"是对的，但它显示的是 `my_ctrl/PurePursuit [my_ctrl::PurePursuit] (base: nav2_core::Controller)`：**方括号前面**的才是加载时要用的名字。

📄 文档说参数"可以是 type 也可以是 name"，这与 pluginlib 6.0.3 的实际行为不符。

**两种改法：**

1. YAML 改成 `plugin: "my_ctrl/PurePursuit"`；
2. 删掉 XML 里的 `name` 属性，查找名就等于 `type`，YAML 保持 `my_ctrl::PurePursuit`。💡 新代码推荐这种：名字和 C++ 类名一致，搜代码时也好找。Nav2 自带插件现在也用 `nav2_xxx::Yyy` 这种 `type` 形式作为插件名。

## 题 2：列表正常，`Declared types are` 为空

`Declared types are` 后面为空，说明 `ClassLoader` 按它的"基类所在包 + 基类名"在 ament 索引里**一个插件描述文件都没找到**（或找到的都不匹配）。而 `ros2 plugin list` 是按 XML 里的 `base_class_type` 分组显示的，不经过 `ClassLoader` 的这一步，所以它正常不说明问题。可能原因：

| 可能原因 | 怎么确认 |
|---|---|
| ① `pluginlib_export_plugin_description_file` 的第一个参数写成了插件包自己（🔍 练习实测就是这个现象） | `ls install/<插件包>/share/ament_index/resource_index/`，应该有 `<基类包>__pluginlib__plugin/` 目录；如果看到的是 `<插件包>__pluginlib__plugin/`，就是写错了 |
| ② 加载方 `ClassLoader` 的第一个参数（基类所在的包）写错了 | 对照代码里 `ClassLoader<T>("包名", "基类名")` 的包名和 ① 里的目录名 `<包名>__pluginlib__plugin` |
| ③ `ClassLoader` 的第二个参数（基类全名）和 XML 的 `base_class_type` 不一致，例如少了命名空间 | 对照 `ros2 plugin list` 里 `(base: ...)` 的字符串和代码里的字符串，必须逐字相同 |

💡 排查顺序：先看 `ros2 plugin list` 的 `base`，再看 ament 索引目录名，最后看 `ClassLoader` 的两个参数。三者对上，`Declared types` 就不会是空的。
