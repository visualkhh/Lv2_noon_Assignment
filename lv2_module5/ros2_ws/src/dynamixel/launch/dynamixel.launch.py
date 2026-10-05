from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os


def generate_launch_description():
    config = os.path.join(get_package_share_directory('dynamixel'), 'config', 'dynamixel.yaml')
    return LaunchDescription([
        Node(package='dynamixel', executable='dynamixel_move_node',
             parameters=[config], output='screen'),
        Node(package='dynamixel', executable='dynamixel_controller',
             parameters=[config], output='screen'),
    ])
