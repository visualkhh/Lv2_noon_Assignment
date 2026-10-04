"""제어 노드만 실행: DynamixelMoveNode + DynamixelController.

use_motor:=false 이면 DynamixelController를 띄우지 않아 모터 출력이 비활성화된다.
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    default_params = os.path.join(
        get_package_share_directory('dynamixel'), 'config', 'dynamixel.yaml')

    params_file = LaunchConfiguration('params_file')
    use_motor = LaunchConfiguration('use_motor')

    return LaunchDescription([
        DeclareLaunchArgument(
            'params_file',
            default_value=default_params,
            description='dynamixel 패키지 파라미터 파일'),
        DeclareLaunchArgument(
            'use_motor',
            default_value='true',
            description='false면 DynamixelController를 실행하지 않음 (모터 출력 비활성)'),

        Node(
            package='dynamixel',
            executable='dynamixel_move_node',
            name='dynamixel_move_node',
            parameters=[params_file],
            output='screen'),

        Node(
            package='dynamixel',
            executable='dynamixel_controller',
            name='dynamixel_controller',
            parameters=[params_file],
            condition=IfCondition(use_motor),
            output='screen'),
    ])
