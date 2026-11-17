#!/usr/bin/env python3
"""从 LTTng 轨迹统计每个回调的执行时长（不依赖 tracetools_analysis）。

用法：python3 callback_durations.py <trace_dir>
原理：ros2:callback_start / ros2:callback_end 成对出现（同一线程 vtid 内），
      ros2:rclcpp_callback_register 把回调地址映射到可读的符号名。
"""
import collections
import statistics
import sys

import bt2


def short(symbol, width=70):
    return symbol if len(symbol) <= width else symbol[:width - 3] + '...'


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 1
    names = {}                                  # 回调地址 -> 符号名
    open_start = {}                             # (vtid, 回调地址) -> 开始时间 ns
    durations = collections.defaultdict(list)   # 回调地址 -> [时长 ns]
    starts = collections.defaultdict(list)      # 回调地址 -> [开始时间 ns]

    for msg in bt2.TraceCollectionMessageIterator(sys.argv[1]):
        if type(msg) is not bt2._EventMessageConst:
            continue
        ev = msg.event
        t = msg.default_clock_snapshot.ns_from_origin
        if ev.name == 'ros2:rclcpp_callback_register':
            names[int(ev['callback'])] = str(ev['symbol'])
        elif ev.name in ('ros2:callback_start', 'ros2:callback_end'):
            key = (int(ev['vtid']), int(ev['callback']))
            if ev.name == 'ros2:callback_start':
                open_start[key] = t
                starts[key[1]].append(t)
            elif key in open_start:
                durations[key[1]].append(t - open_start.pop(key))

    for cb, ds in sorted(durations.items(), key=lambda kv: -len(kv[1])):
        st = starts[cb]
        period = statistics.mean(b - a for a, b in zip(st, st[1:])) / 1e6 if len(st) > 1 else 0.0
        print(f'{len(ds):4d} calls  mean {statistics.mean(ds) / 1e6:7.3f} ms  '
              f'max {max(ds) / 1e6:7.3f} ms  period {period:7.1f} ms  '
              f'{short(names.get(cb, hex(cb)))}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
