# 规划结果的简单状态消息（演示：常量、默认值、有界数组、嵌套类型）
# 引用同一个包里的消息时不写包名：直接写 Pose2D
uint8 STATUS_OK=0
uint8 STATUS_NO_PATH=1
uint8 STATUS_TIMEOUT=2

std_msgs/Header header
uint8 status 0
float32 length_m 0.0
Pose2D[<=1000] waypoints
