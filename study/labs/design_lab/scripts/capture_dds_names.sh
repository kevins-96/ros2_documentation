#!/usr/bin/env bash
# 抓 RTPS 发现报文，列出 ROS 名字在 DDS 里的真实名字和类型名。
# 需要 tcpdump 和 root 权限（容器里默认是 root）。
# 用法：capture_dds_names.sh [秒数，默认 6]
set -euo pipefail
secs="${1:-6}"
pcap="$(mktemp --suffix=.pcap)"
timeout "${secs}" tcpdump -i any -w "${pcap}" udp >/dev/null 2>&1 || true
echo "== DDS 话题名（rt/ rq/ rr/ 前缀，以及不带前缀的）=="
strings -n 4 "${pcap}" | grep -E '^(rt|rq|rr)/|^ros_discovery_info$|^raw_dds_topic$|^/raw_dds_topic$' | sort -u
echo "== DDS 类型名（包含 dds_）=="
strings -n 4 "${pcap}" | grep -E 'dds_::' | sort -u
rm -f "${pcap}"
