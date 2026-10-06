"""
인지 실행: realsense2_camera (Intel 공식 래퍼) + PerceptionNode.

PerceptionNode의 토픽 이름은 파라미터다 (기본값은 params_file, 즉 config/realsense.yaml).
launch 인자 image_topic·target_topic을 주면 그 값이 params_file보다 우선한다.

예시:
  ros2 launch realsense realsense.launch.py
  ros2 launch realsense realsense.launch.py image_topic:=/camera/color/image_raw
  ros2 launch realsense realsense.launch.py use_camera:=false target_topic:=/target_replay
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription, OpaqueFunction
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare

# launch 인자로 덮어쓸 수 있는 토픽 파라미터 (빈 문자열이면 params_file 값을 쓴다)
TOPIC_ARGS = ('image_topic', 'target_topic')


def perception_node(context):
    overrides = {}
    for name in TOPIC_ARGS:
        value = LaunchConfiguration(name).perform(context)
        if value:
            overrides[name] = value
    parameters = [LaunchConfiguration('params_file')]
    if overrides:
        parameters.append(overrides)
    return [Node(
        package='realsense',
        executable='perception_node',
        name='perception_node',
        parameters=parameters,
        output='screen')]


def generate_launch_description():
    default_params = os.path.join(
        get_package_share_directory('realsense'), 'config', 'realsense.yaml')

    use_camera = LaunchConfiguration('use_camera')
    color_profile = LaunchConfiguration('color_profile')

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
            default_value='424x240x30',
            description='RealSense 컬러 해상도·FPS (폭x높이xFPS). 기본 424x240x30: 영상 1장 약 300KB로 '
                        '640x480(약 900KB)보다 전송·처리 부담이 작고 1m 기둥도 검출됨 (README 참고)'),
        DeclareLaunchArgument(
            'image_topic',
            default_value='',
            description='PerceptionNode가 구독할 카메라 컬러 토픽 '
                        '(비우면 params_file의 image_topic, 기본 /camera/camera/color/image_raw)'),
        DeclareLaunchArgument(
            'target_topic',
            default_value='',
            description='PerceptionNode가 발행할 목표 토픽 '
                        '(비우면 params_file의 target_topic, 기본 /target)'),

        camera,
        OpaqueFunction(function=perception_node),
    ])
