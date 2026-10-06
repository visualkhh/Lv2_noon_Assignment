"""가상환경 ↔ 실기 자동 전환 + 웹 GUI 연결.

  ros2 launch cognitive_control sim.launch.py      # 이것 하나로 끝 (기기 연결·분리는 자동)

  카메라  RealSense가 없으면 virtual_world가 가상 영상 발행
          USB에 꽂히면 device_manager가 realsense2_camera 실행 → virtual_world는 멈춤
  인지    perception: 영상(가상·실기 공통) → /target, 디버그·마스크 영상
  제어    tracker: /target → /motor_cmd, /tracking_status
  모터    opencr_bridge: OpenCR 포트가 생기면 연결, 실제 카메라일 때만 /motor_cmd 를 시리얼로 전송
  GUI     rosbridge (ws://localhost:9090) → lv2_module5/index.html

  use_tracker:=false  제어를 다른 노드(팀 dynamixel_move_node 등)가 맡을 때
  use_devices:=false  기기 자동 연결(device_manager, opencr_bridge) 끄기 — 가상환경만
"""

from ament_index_python.packages import get_package_share_directory, PackageNotFoundError
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription, LogInfo
from launch.conditions import IfCondition
from launch.launch_description_sources import AnyLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def rosbridge():
    try:
        share = get_package_share_directory('rosbridge_server')
    except PackageNotFoundError:
        return LogInfo(msg='rosbridge_server 없음 → GUI 연결 불가. '
                           'sudo apt install ros-jazzy-rosbridge-suite')
    return IncludeLaunchDescription(
        AnyLaunchDescriptionSource(f'{share}/launch/rosbridge_websocket_launch.xml'),
        launch_arguments={'port': LaunchConfiguration('port')}.items())


def node(executable, condition=None):
    return Node(package='cognitive_control', executable=executable, output='screen',
                condition=IfCondition(LaunchConfiguration(condition)) if condition else None)


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument('use_sim', default_value='true',
                              description='false면 virtual_world를 띄우지 않음'),
        DeclareLaunchArgument('use_tracker', default_value='true',
                              description='false면 tracker를 띄우지 않음 (다른 제어 노드 사용)'),
        DeclareLaunchArgument('use_devices', default_value='true',
                              description='false면 RealSense·OpenCR 자동 연결을 하지 않음'),
        DeclareLaunchArgument('port', default_value='9090', description='rosbridge 포트'),
        node('virtual_world', 'use_sim'),
        node('perception'),
        node('tracker', 'use_tracker'),
        node('device_manager', 'use_devices'),
        node('opencr_bridge', 'use_devices'),
        rosbridge(),
    ])
