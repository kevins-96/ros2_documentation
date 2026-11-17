"""Launch smoother_node with a plugin from this package and check the published path."""
import unittest

import launch
import launch_ros.actions
import launch_testing.actions
import launch_testing.asserts
from launch_testing_ros.actions import EnableRmwIsolation
from nav_msgs.msg import Path
import pytest
import rclpy
from rclpy.qos import DurabilityPolicy, QoSProfile


@pytest.mark.launch_test
def generate_test_description():
    node = launch_ros.actions.Node(
        package='path_smoother_core',
        executable='smoother_node',
        parameters=[{'smoother_plugin': 'moving_average', 'points': 11, 'smoother.window': 5}],
    )
    return launch.LaunchDescription([
        # 用 rmw_test_fixture 隔离通信：并行跑的其他测试看不到这里的话题
        EnableRmwIsolation(),
        node,
        launch_testing.actions.ReadyToTest(),
    ]), {'smoother': node}


class TestSmoothedPath(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        rclpy.init()

    @classmethod
    def tearDownClass(cls):
        rclpy.shutdown()

    def test_path_is_smoothed(self, proc_output):
        node = rclpy.create_node('checker')
        got = []
        qos = QoSProfile(depth=1, durability=DurabilityPolicy.TRANSIENT_LOCAL)
        node.create_subscription(Path, 'smoothed_path', got.append, qos)
        try:
            end = node.get_clock().now().nanoseconds + 10 * 10**9
            while not got and node.get_clock().now().nanoseconds < end:
                rclpy.spin_once(node, timeout_sec=0.1)
        finally:
            node.destroy_node()
        self.assertEqual(len(got), 1, 'no path received within 10 s')
        ys = [p.pose.position.y for p in got[0].poses]
        self.assertEqual(len(ys), 11)
        self.assertAlmostEqual(ys[0], 0.0)
        # 平滑后中间点不应再是 0/1 来回跳
        self.assertTrue(all(0.2 < y < 0.8 for y in ys[2:-2]), ys)
        proc_output.assertWaitFor('plugin=moving_average', timeout=5)


@launch_testing.post_shutdown_test()
class TestShutdown(unittest.TestCase):

    def test_exit_code(self, proc_info):
        # SIGINT 结束的节点退出码是 0；launch_testing 允许的退出码见 EXIT_OK
        launch_testing.asserts.assertExitCodes(proc_info)
