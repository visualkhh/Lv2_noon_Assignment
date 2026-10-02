"""인지 실행: realsense2_camera (Intel 공식 래퍼) + PerceptionNode.

PerceptionNode는 /image_raw를 구독하며, 여기서 카메라 컬러 토픽으로 remap한다.
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    default_params = os.path.join(
        get_package_share_directory('realsense'), 'config', 'realsense.yaml')

    params_file = LaunchConfiguration('params_file')
    use_camera = LaunchConfiguration('use_camera')
    color_profile = LaunchConfiguration('color_profile')
    image_topic = LaunchConfiguration('image_topic')

    camera = IncludeLaunchDescription(
        # use_camera:=false 이면 경로를 찾지 않으므로 realsense2_camera 없이도 실행 가능
        PythonLaunchDescriptionSource(PathJoinSubstitution(
            [FindPackageShare('realsense2_camera'), 'launch', 'rs_launch.py'])),
        launch_arguments={
            'enable_color': 'true',
            'enable_depth': 'false',
            'enable_infra1': 'false',
            'enable_infra2': 'false',
            'rgb_camera.color_profile': color_profile,
        }.items(),
        condition=IfCondition(use_camera))

    perception = Node(
        package='realsense',
        executable='perception_node',
        name='perception_node',
        parameters=[params_file],
        remappings=[('/image_raw', image_topic)],
        output='screen')

    return LaunchDescription([
        DeclareLaunchArgument(
            'params_file',
            default_value=default_params,
            description='perception_node 파라미터 파일'),
        DeclareLaunchArgument(
            'use_camera',
            default_value='true',
            description='false면 카메라를 실행하지 않음 (bag 재생으로 입력할 때)'),
        DeclareLaunchArgument(
            'color_profile',
            default_value='640x480x30',
            description='RealSense 컬러 해상도·FPS (폭x높이xFPS)'),
        DeclareLaunchArgument(
            'image_topic',
            default_value='/camera/camera/color/image_raw',
            description='PerceptionNode가 구독할 카메라 컬러 토픽'),

        camera,
        perception,
    ])
