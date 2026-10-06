"""RealSense 자동 연결: USB에 꽂히면 realsense2_camera를 띄우고, 뽑히면 내린다.

/sys/bus/usb/devices 를 주기적으로 보고 Intel(8086) RealSense 장치가 있으면
  ros2 launch realsense2_camera rs_launch.py  (토픽: /camera/camera/color/image_raw)
를 하위 프로세스로 실행한다. 카메라 토픽이 생기면 virtual_world가 스스로 가상 영상을 멈춘다.
"""

import os
from pathlib import Path
import signal
import subprocess

import rclpy
from rclpy.executors import ExternalShutdownException
from rclpy.node import Node


def realsense_present():
    for dev in Path('/sys/bus/usb/devices').glob('*'):
        try:
            vendor = (dev / 'idVendor').read_text().strip()
            product = (dev / 'product').read_text().strip()
        except OSError:
            continue
        if vendor == '8086' and 'realsense' in product.lower():
            return True
    return False


class DeviceManager(Node):

    def __init__(self):
        super().__init__('device_manager')
        self.launch_args = list(self.declare_parameter(
            'realsense_args',
            ['enable_depth:=false', 'rgb_camera.color_profile:=640,480,30']).value)
        self.proc = None
        self.create_timer(self.declare_parameter('poll_period', 2.0).value, self.poll)
        self.get_logger().info('device_manager 시작 (RealSense USB 연결 감시)')

    def poll(self):
        present = realsense_present()
        running = self.proc is not None and self.proc.poll() is None
        if present and not running:
            self.get_logger().info('RealSense 감지 → realsense2_camera 실행')
            self.proc = subprocess.Popen(
                ['ros2', 'launch', 'realsense2_camera', 'rs_launch.py', *self.launch_args],
                start_new_session=True)
        elif not present and running:
            self.get_logger().info('RealSense 분리 → realsense2_camera 종료')
            self.stop()

    def stop(self):
        if self.proc is None or self.proc.poll() is not None:
            return
        os.killpg(self.proc.pid, signal.SIGINT)
        try:
            self.proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            os.killpg(self.proc.pid, signal.SIGKILL)
        self.proc = None


def main(args=None):
    rclpy.init(args=args)
    node = DeviceManager()
    try:
        rclpy.spin(node)
    except (KeyboardInterrupt, ExternalShutdownException):
        pass
    finally:
        node.stop()
        rclpy.try_shutdown()
