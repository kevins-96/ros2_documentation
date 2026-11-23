#!/usr/bin/env bash
# 第 9 批：记录自己收到 SIGINT / SIGTERM 的时刻，并且故意不退出，用来观察 launch 的信号升级。
# 用法：signal_logger.sh <日志文件> [ignore|exit_on_int]
log=${1:-/tmp/signal_logger.log}
mode=${2:-ignore}
start=$(date +%s.%N)
echo "start $start pid=$$" > "$log"
on_sig() {
  now=$(date +%s.%N)
  awk -v n="$now" -v s="$start" -v sig="$1" 'BEGIN{printf "%s %.2f s\n", sig, n-s}' >> "$log"
  if [ "$mode" = exit_on_int ] && [ "$1" = SIGINT ]; then exit 0; fi
}
trap 'on_sig SIGINT' INT
trap 'on_sig SIGTERM' TERM
while true; do sleep 0.05; done
