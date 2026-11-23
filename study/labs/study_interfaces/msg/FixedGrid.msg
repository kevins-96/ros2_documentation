# 定长栅格（练习用）：演示定长数组与字段默认值在生成 C++ 代码里的效果
# 见 design_lab/src/msg_init_bench.cpp
uint32 width
uint32 height
float64 resolution 0.05        # 默认值：用 MessageInitialization::ALL 构造时生效
float32[262144] cells          # 512 x 512，定长数组 -> std::array<float, 262144>（约 1 MB）
string<=8 tag                  # 有界字符串：C++ 里仍是 std::string；实测 Fast DDS 序列化时也不检查长度
