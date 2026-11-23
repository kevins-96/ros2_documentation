# 第 9 批：用生命周期事件编排启动顺序（150_roslaunch 的 Deterministic Startup 一节）。
# lidar_driver 是生命周期节点；只有它进入 active 之后，才启动依赖它的 consumer（这里用 remap_probe 代替）。
# 用法：ros2 launch design_lab lifecycle_chain.launch.py
from launch import LaunchDescription
from launch.actions import EmitEvent, LogInfo, RegisterEventHandler
from launch.events import matches_action
from launch_ros.actions import LifecycleNode, Node
from launch_ros.event_handlers import OnStateTransition
from launch_ros.events.lifecycle import ChangeState
from lifecycle_msgs.msg import Transition


def generate_launch_description():
    driver = LifecycleNode(package='design_lab', executable='lifecycle_design_probe',
                           name='lidar_driver', namespace='', arguments=['--serve'],
                           output='screen')
    configure = EmitEvent(event=ChangeState(
        lifecycle_node_matcher=matches_action(driver),
        transition_id=Transition.TRANSITION_CONFIGURE))
    activate_when_inactive = RegisterEventHandler(OnStateTransition(
        target_lifecycle_node=driver, goal_state='inactive',
        entities=[LogInfo(msg='driver 已 inactive，请求 activate'),
                  EmitEvent(event=ChangeState(
                      lifecycle_node_matcher=matches_action(driver),
                      transition_id=Transition.TRANSITION_ACTIVATE))]))
    start_consumer_when_active = RegisterEventHandler(OnStateTransition(
        target_lifecycle_node=driver, goal_state='active',
        entities=[LogInfo(msg='driver 已 active，启动 consumer'),
                  Node(package='design_lab', executable='remap_probe', name='consumer',
                       output='screen')]))
    return LaunchDescription([
        activate_when_inactive, start_consumer_when_active, driver, configure,
    ])
