"""전체 실행: realsense.launch.py(카메라 + PerceptionNode) + dynamixel.launch.py(제어).

예시:
  ros2 launch bringup bringup.launch.py
  ros2 launch bringup bringup.launch.py use_camera:=false   # 카메라 대신 다른 영상 입력 (fake_camera_bringup 등)
  ros2 launch bringup bringup.launch.py use_motor:=false    # 모터 출력 없이
  ros2 launch bringup bringup.launch.py dynamixel_params_file:=/path/to/kp_a.yaml
"""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, GroupAction, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare


def include(package, launch_file, args):
    # GroupAction으로 감싸 launch 인자 범위를 분리한다.
    # 안 감싸면 두 launch가 같이 쓰는 인자명(params_file)이 먼저 선언된 쪽 값으로 새어
    # dynamixel 노드가 realsense.yaml을 받는다.
    return GroupAction([IncludeLaunchDescription(
        PythonLaunchDescriptionSource(PathJoinSubstitution(
            [FindPackageShare(package), 'launch', launch_file])),
        launch_arguments={a: LaunchConfiguration(a) for a in args}.items())])


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument(
            'use_camera', default_value='true',
            description='false면 realsense2_camera를 띄우지 않음'),
        DeclareLaunchArgument(
            'use_motor', default_value='true',
            description='false면 DynamixelController를 띄우지 않음'),
        DeclareLaunchArgument(
            'dynamixel_params_file',
            default_value=PathJoinSubstitution(
                [FindPackageShare('dynamixel'), 'config', 'dynamixel.yaml']),
            description='제어 설정 파일 (문제 3의 Kp A/B 선택)'),
        include('realsense', 'realsense.launch.py', ['use_camera']),
        GroupAction([IncludeLaunchDescription(
            PythonLaunchDescriptionSource(PathJoinSubstitution(
                [FindPackageShare('dynamixel'), 'launch', 'dynamixel.launch.py'])),
            launch_arguments={
                'use_motor': LaunchConfiguration('use_motor'),
                'params_file': LaunchConfiguration('dynamixel_params_file'),
            }.items())]),
    ])
