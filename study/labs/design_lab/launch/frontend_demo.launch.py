# 第 9 批：与 frontend_demo.launch.xml 等价的 Python 写法，用 --print 对比两者的描述。
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, GroupAction, SetLaunchConfiguration
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration, PythonExpression
from launch_ros.actions import Node, PushROSNamespace


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument('robot', default_value='robot1', description='机器人命名空间'),
        DeclareLaunchArgument('use_lidar', default_value='true', description='是否启动激光相关节点'),
        SetLaunchConfiguration('ns', ['/', LaunchConfiguration('robot')]),
        GroupAction([
            PushROSNamespace(LaunchConfiguration('ns')),
            Node(package='design_lab', executable='remap_probe', name='planner',
                 condition=IfCondition(LaunchConfiguration('use_lidar')), output='screen',
                 parameters=[{'max_speed': 1.0,
                              'frame_ids': ['map', 'odom', 'base_link'],
                              'limits.accel': PythonExpression(['0.5 * 2'])}],
                 remappings=[('scan', 'scan_filtered')]),
        ]),
    ])
