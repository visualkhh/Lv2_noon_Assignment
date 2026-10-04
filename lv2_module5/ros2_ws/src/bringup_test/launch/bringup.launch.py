from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription([
        Node(
            package='bringup_test',
            executable='talker',
            name='bringup_talker',
        ),
        Node(
            package='bringup_test',
            executable='listener',
            name='bringup_listener',
        ),
    ])
