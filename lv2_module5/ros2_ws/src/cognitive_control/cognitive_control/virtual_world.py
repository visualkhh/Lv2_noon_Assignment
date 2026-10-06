"""가상 카메라: 실제 카메라가 없을 때만 /camera/camera/color/image_raw 를 대신 발행한다.

pan/tilt 카메라가 파란 사각 목표가 움직이는 가상 공간을 본다.
/motor_cmd(관절 변화량)를 누적해 카메라 방향을 바꾸므로 perception·tracker와 닫힌 루프가 된다.
같은 토픽에 다른 노드(realsense2_camera)가 발행을 시작하면 가상 영상을 멈추고, 사라지면 다시 발행한다.

발행
  /camera/camera/color/image_raw  sensor_msgs/Image (rgb8, frame_id=virtual_camera_optical_frame)
  /camera_source                  std_msgs/String  "virtual" | "realsense"
구독
  /motor_cmd  sensor_msgs/JointState  position = [Δpan, Δtilt] rad
"""

import math

import cv2
import numpy as np
import rclpy
from rclpy.executors import ExternalShutdownException
from rclpy.node import Node
from sensor_msgs.msg import Image, JointState
from std_msgs.msg import String

IMAGE_TOPIC = '/camera/camera/color/image_raw'


class VirtualWorld(Node):

    def __init__(self):
        super().__init__('virtual_world')
        self.width = self.declare_parameter('width', 640).value
        self.height = self.declare_parameter('height', 480).value
        fps = self.declare_parameter('fps', 15.0).value
        hfov = math.radians(self.declare_parameter('hfov_deg', 69.0).value)
        self.target_size = self.declare_parameter('target_size_rad', 0.08).value
        self.amp_az = self.declare_parameter('amp_az_rad', 0.5).value
        self.amp_el = self.declare_parameter('amp_el_rad', 0.2).value
        self.speed = self.declare_parameter('speed', 0.2).value
        # hide_period초마다 hide_duration초 동안 목표를 숨겨 LOST 상황을 만든다 (0이면 안 숨김)
        self.hide_period = self.declare_parameter('hide_period', 20.0).value
        self.hide_duration = self.declare_parameter('hide_duration', 3.0).value

        self.focal = (self.width / 2) / math.tan(hfov / 2)
        self.pan = 0.0
        self.tilt = 0.0
        self.t0 = self.get_clock().now()
        self.source = 'virtual'

        self.raw_pub = self.create_publisher(Image, IMAGE_TOPIC, 10)
        self.source_pub = self.create_publisher(String, '/camera_source', 10)
        self.create_subscription(JointState, '/motor_cmd', self.on_motor_cmd, 10)
        self.create_timer(1.0 / fps, self.step)
        self.create_timer(1.0, self.check_source)
        self.get_logger().info('virtual_world 시작 (실제 카메라가 없으면 가상 영상 발행)')

    def check_source(self):
        others = [p for p in self.get_publishers_info_by_topic(IMAGE_TOPIC)
                  if p.node_name != self.get_name()]
        source = 'realsense' if others else 'virtual'
        if source != self.source:
            self.get_logger().info(f'카메라 전환: {self.source} → {source}')
            self.source = source
        self.source_pub.publish(String(data=self.source))

    def on_motor_cmd(self, msg):
        if self.source == 'virtual' and len(msg.position) >= 2:
            self.pan += msg.position[0]
            self.tilt += msg.position[1]

    def target_angles(self, t):
        az = self.amp_az * math.sin(self.speed * t)
        el = self.amp_el * math.sin(self.speed * 1.55 * t)
        visible = not (self.hide_period > 0 and
                       t % self.hide_period > self.hide_period - self.hide_duration)
        return az, el, visible

    def project(self, az, el):
        # pan +: 왼쪽으로 회전, tilt +: 아래로 숙임 (실기 제어 노드의 부호와 같음)
        u = self.width / 2 - self.focal * math.tan(az - self.pan)
        v = self.height / 2 - self.focal * math.tan(el + self.tilt)
        return u, v

    def render(self, t):
        w, h = self.width, self.height
        img = np.full((h, w, 3), (205, 200, 190), np.uint8)    # BGR
        # 바닥·벽 경계와 세로 기둥선: 카메라가 돌면 배경도 움직여 보이게 한다
        _, horizon = self.project(0.0, -0.15)
        cv2.rectangle(img, (0, int(np.clip(horizon, 0, h))), (w, h), (150, 160, 170), -1)
        for k in range(-12, 13):
            u, _ = self.project(k * 0.15, 0.0)
            if 0 <= u < w:
                cv2.line(img, (int(u), 0), (int(u), h), (175, 175, 170), 1)
        az, el, visible = self.target_angles(t)
        if visible and abs(az - self.pan) < 1.2 and abs(el + self.tilt) < 1.2:
            u, v = self.project(az, el)
            half = self.focal * self.target_size / 2
            cv2.rectangle(img, (int(u - half), int(v - 2 * half)),
                          (int(u + half), int(v + 2 * half)), (200, 90, 20), -1)
        cv2.putText(img, f'VIRTUAL  pan={math.degrees(self.pan):+.1f} '
                         f'tilt={math.degrees(self.tilt):+.1f}',
                    (10, h - 12), cv2.FONT_HERSHEY_SIMPLEX, 0.55, (20, 20, 20), 1)
        return img

    def step(self):
        if self.source != 'virtual':
            return
        now = self.get_clock().now()
        img = self.render((now - self.t0).nanoseconds * 1e-9)
        raw = Image()
        raw.header.stamp = now.to_msg()
        raw.header.frame_id = 'virtual_camera_optical_frame'
        raw.height, raw.width = img.shape[:2]
        raw.encoding = 'rgb8'
        raw.step = raw.width * 3
        raw.data = cv2.cvtColor(img, cv2.COLOR_BGR2RGB).tobytes()
        self.raw_pub.publish(raw)


def main(args=None):
    rclpy.init(args=args)
    try:
        rclpy.spin(VirtualWorld())
    except (KeyboardInterrupt, ExternalShutdownException):
        pass
    finally:
        rclpy.try_shutdown()
