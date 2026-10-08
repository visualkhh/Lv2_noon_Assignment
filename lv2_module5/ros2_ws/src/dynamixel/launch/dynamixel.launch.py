from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os


def generate_launch_description():
    config = os.path.join(get_package_share_directory('dynamixel'), 'config', 'dynamixel.yaml')
    return LaunchDescription([
        DeclareLaunchArgument('params_file', default_value=config,
                              description='dynamixel_move_node와 controller의 설정 파일'),
        DeclareLaunchArgument('use_motor', default_value='true',
                              description='false면 OpenCR에 쓰는 controller를 실행하지 않음'),
        Node(package='dynamixel', executable='dynamixel_move_node',
             parameters=[LaunchConfiguration('params_file')], output='screen'),
        Node(package='dynamixel', executable='dynamixel_controller',
             parameters=[LaunchConfiguration('params_file')],
             condition=IfCondition(LaunchConfiguration('use_motor')), output='screen'),
    ])
