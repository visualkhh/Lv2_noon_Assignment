"""가상환경 + 웹 GUI 연결.

  ros2 launch cognitive_control sim.launch.py                    # 가상 카메라 + 제어 + rosbridge
  ros2 launch cognitive_control sim.launch.py use_sim:=false     # 실기 노드가 따로 돌 때 rosbridge만
  ros2 launch cognitive_control sim.launch.py use_tracker:=false # 제어는 다른 노드가 맡을 때

GUI: lv2_module5/index.html 을 브라우저로 열면 ws://localhost:9090 으로 붙는다.
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


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument('use_sim', default_value='true',
                              description='false면 virtual_world를 띄우지 않음 (실기 카메라·인지 사용)'),
        DeclareLaunchArgument('use_tracker', default_value='true',
                              description='false면 tracker를 띄우지 않음 (실기 제어 노드 사용)'),
        DeclareLaunchArgument('port', default_value='9090', description='rosbridge 포트'),
        Node(package='cognitive_control', executable='virtual_world', output='screen',
             condition=IfCondition(LaunchConfiguration('use_sim'))),
        Node(package='cognitive_control', executable='tracker', output='screen',
             condition=IfCondition(LaunchConfiguration('use_tracker'))),
        rosbridge(),
    ])
