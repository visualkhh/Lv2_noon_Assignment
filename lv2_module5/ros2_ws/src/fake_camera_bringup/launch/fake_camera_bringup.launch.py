"""테스트베드 실행: bringup(인지+제어, 실카메라 없이) + 더미 카메라 + 모니터 매니저.

더미 카메라는 realsense2_camera와 같은 namespace/name(camera/camera)으로 띄워
/camera/camera/color/image_raw · camera_info 토픽 이름을 그대로 맞춘다.
모터 명령은 dynamixel.yaml의 /dev/ttyACM0로 나가고, 테스트베드 컨테이너에선
entrypoint가 /dev/ttyACM0 → 가상 시리얼(/dev/ttyV0)로 연결해 debug/serial-out에 쌓인다.
모니터 매니저는 이미지 토픽(마스크·디버그·카메라)을 debug/topic/<토픽>/image.jpg 로 저장한다.

예시:
  ros2 launch fake_camera_bringup fake_camera_bringup.launch.py
  ros2 launch fake_camera_bringup fake_camera_bringup.launch.py image_dir:=/ws/debug/other period_s:=1.0
"""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument(
            'image_dir', default_value='/ws/debug/input-images',
            description='숫자 이름 이미지 폴더 (1.png, 2.png ...)'),
        DeclareLaunchArgument(
            'period_s', default_value='0.1',
            description='이미지 1장 발행 간격 [s]. target_timeout(0.5s)보다 짧아야 TRACKING 유지'),

        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(PathJoinSubstitution(
                [FindPackageShare('bringup'), 'launch', 'bringup.launch.py'])),
            launch_arguments={'use_camera': 'false'}.items()),

        Node(
            package='fake_camera_bringup',
            executable='fake_camera',
            namespace='camera',
            name='camera',
            parameters=[{
                'image_dir': LaunchConfiguration('image_dir'),
                'period_s': LaunchConfiguration('period_s'),
            }],
            output='screen'),

        Node(
            package='fake_camera_bringup',
            executable='monitor_manager',
            name='monitor_manager',
            output='screen'),
    ])
