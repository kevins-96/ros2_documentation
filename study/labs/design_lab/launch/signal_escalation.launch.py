# 第 9 批：launch 关闭子进程时的信号升级（150_roslaunch 的 Termination 一节）。
# 子进程 signal_logger.sh 忽略 SIGINT 和 SIGTERM，把收到信号的时刻写进日志文件。
# 用法：ros2 launch design_lab signal_escalation.launch.py log:=/tmp/esc.log
#      另开终端对 launch 进程发 SIGINT 或 SIGTERM，然后看日志文件。
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.substitutions import FindPackagePrefix


def generate_launch_description():
    log = LaunchConfiguration('log')
    script = PathJoinSubstitution(
        [FindPackagePrefix('design_lab'), 'lib', 'design_lab', 'signal_logger.sh'])
    return LaunchDescription([
        DeclareLaunchArgument('log', default_value='/tmp/esc.log',
                              description='signal_logger 写日志的位置'),
        DeclareLaunchArgument('sigterm_timeout', default_value='5',
                              description='SIGINT 之后多久发 SIGTERM（秒）'),
        DeclareLaunchArgument('sigkill_timeout', default_value='5',
                              description='SIGTERM 之后多久发 SIGKILL（秒）'),
        ExecuteProcess(cmd=[script, log, 'ignore'], name='stubborn', output='screen',
                       sigterm_timeout=LaunchConfiguration('sigterm_timeout'),
                       sigkill_timeout=LaunchConfiguration('sigkill_timeout')),
    ])
