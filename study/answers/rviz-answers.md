# 答案：RViz 与可视化

> 先在[卡片](../cards/observability/rviz.md)里自己写下答案再来对照。

## 题 1：5 条候选轨迹只看到 1 条，偶尔一闪就没

**原因 1：5 条用了同一个 `ns` + `id`。** 同一标识的新 Marker 会替换旧的，最后只剩一条。确认：

```bash
ros2 topic echo --once /local_plan_markers --field markers | grep -E "ns:|id:"
```

看 5 个元素的 `id` 是不是相同。

**原因 2：其中几条 alpha 是 0 或颜色没设。** 🔍 `Marker` 默认 `color` 全 0，全透明。确认：`--field markers` 里看每个元素 `color` 的 `a`。

**原因 3：`lifetime` 太短，发布频率比 lifetime 低。** 例如 `lifetime` 0.1 s、发布 5 Hz，每次显示 0.1 s 后消失，看起来"一闪就没"。确认：`--field markers` 看 `lifetime`，再 `ros2 topic hz` 看实际发布频率。

**原因 4：`frame_id` 有问题，部分 Marker 无法变换。** 比如有的写了 `/odom`（🔍 tf2 拒绝以 `/` 开头的名字），或者用了一个只有时不时才有 TF 的坐标系。确认：`--field markers` 看 `header.frame_id`；RViz 里展开 Marker Display 的 `Status`。

**原因 5：`orientation` 全 0。** 每条线的 `pose.orientation` 没设，部分 Marker 被 RViz 拒绝。确认：看 `pose.orientation.w`。

💡 还要留意 RViz 那边：Marker Display 下面可以按 namespace 勾选显示，确认没把某个 namespace 关掉。

## 题 2：Fixed Frame 是 `base_link`，转弯时点云拖出弧线

**原因：** 📄 Fixed Frame 应该相对世界静止。设成 `base_link` 后，RViz 把每一帧点云变换到"当时的 `base_link`"里，并保持在屏幕上（点云 Display 的 Decay Time 不为 0 时还会累积多帧）。机器人直行时，各帧之间 `base_link` 只是平移，看起来差别不大；转弯时，每一帧的 `base_link` 朝向都不一样，同一面墙在不同帧里落在不同位置，累积起来就成了一条弧。

**改法：** Fixed Frame 改成 `odom` 或 `map`（相对世界静止的坐标系）。如果想让视角跟着机器人，把 **Target Frame**（Views 面板）设成 `base_link`，这只影响相机，不影响数据怎么变换。

💡 回放 bag 时还要确认 RViz 用了 `use_sim_time:=true`、bag 带 `--clock`，否则 RViz 用当前时间查 tf，可能出现 `No transform` 或数据被丢弃。
