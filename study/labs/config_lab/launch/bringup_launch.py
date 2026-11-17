# 与 bringup_launch.xml 等价的 Python 版本，另外多了一个事件处理：planner 退出时整体关闭
from launch import LaunchDescription
from launch.actions import (DeclareLaunchArgument, EmitEvent, GroupAction,
                            IncludeLaunchDescription, LogInfo, RegisterEventHandler)
from launch.conditions import IfCondition
from launch.event_handlers import OnProcessExit
from launch.events import Shutdown
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node, PushROSNamespace
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    robot_ns = LaunchConfiguration('robot_ns')
    use_sim_time = LaunchConfiguration('use_sim_time')
    enable_lidar = LaunchConfiguration('enable_lidar')
    log_level = LaunchConfiguration('log_level')
    share = FindPackageShare('config_lab')

    planner = Node(
        package='config_lab', executable='card_parameter_min', name='planner',
        output='screen',
        ros_arguments=['--log-level', log_level],
        parameters=[PathJoinSubstitution([share, 'config', 'planner.yaml']),
                    {'use_sim_time': use_sim_time}],   # 列表里后面的覆盖前面的
    )

    return LaunchDescription([
        DeclareLaunchArgument('robot_ns', default_value='robot1'),
        DeclareLaunchArgument('use_sim_time', default_value='false'),
        DeclareLaunchArgument('enable_lidar', default_value='true'),
        DeclareLaunchArgument('log_level', default_value='info'),
        GroupAction([
            PushROSNamespace(robot_ns),               # 注意：传的是 LaunchConfiguration，不是字符串
            planner,
            Node(package='config_lab', executable='name_lab', name='lidar',
                 arguments=['--spin'], remappings=[('scan', 'scan_front')], output='screen'),
            IncludeLaunchDescription(
                PathJoinSubstitution([share, 'launch', 'lidar_launch.xml']),
                condition=IfCondition(enable_lidar)),
        ]),
        RegisterEventHandler(OnProcessExit(
            target_action=planner,
            on_exit=[LogInfo(msg='planner exited, shutting down the whole system'),
                     EmitEvent(event=Shutdown(reason='planner exited'))])),
    ])
