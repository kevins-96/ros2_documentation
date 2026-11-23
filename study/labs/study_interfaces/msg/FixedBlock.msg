# 定长、没有字符串和变长数组的消息（练习用）：只有这种"平坦"类型才能借用（loaned message）
# 见 design_lab/src/ipc_design_probe.cpp、rmw_probe.cpp
uint64 seq
float32[262144] data           # 1 MB，定长数组 -> std::array<float, 262144>
