"""가상 카메라 + 인지: 장비 없이 실기와 같은 토픽을 발행한다.

pan/tilt 카메라가 파란 사각 목표가 움직이는 가상 공간을 본다.
/motor_cmd(관절 변화량)를 누적해 카메라 방향을 바꾸므로 제어 노드와 닫힌 루프가 된다.

발행 (실기 노드와 같은 이름·형식)
  /camera/camera/color/image_raw            sensor_msgs/Image (rgb8)
  /perception_node/debug_image/compressed   sensor_msgs/CompressedImage (jpeg)
  /perception_node/mask/compressed          sensor_msgs/CompressedImage (jpeg)
  /target   geometry_msgs/PointStamped  x,y = 화면 중심 정규화 오차 [-1,1], z = 면적 비율
            미검출이면 x=y=z=0
구독
  /motor_cmd  sensor_msgs/JointState  position = [Δpan, Δtilt] rad
"""

import math

import cv2
from geometry_msgs.msg import PointStamped
import numpy as np
import rclpy
from rclpy.executors import ExternalShutdownException
from rclpy.node import Node
from sensor_msgs.msg import CompressedImage, Image, JointState

BLUE_LOW = np.array([100, 120, 60])     # HSV, 실기 perception과 같은 계열의 파란색 범위
BLUE_HIGH = np.array([130, 255, 255])


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
        self.publish_raw = self.declare_parameter('publish_raw', True).value

        self.focal = (self.width / 2) / math.tan(hfov / 2)
        self.pan = 0.0
        self.tilt = 0.0
        self.t0 = self.get_clock().now()

        self.raw_pub = self.create_publisher(Image, '/camera/camera/color/image_raw', 10)
        self.debug_pub = self.create_publisher(
            CompressedImage, '/perception_node/debug_image/compressed', 10)
        self.mask_pub = self.create_publisher(
            CompressedImage, '/perception_node/mask/compressed', 10)
        self.target_pub = self.create_publisher(PointStamped, '/target', 10)
        self.create_subscription(JointState, '/motor_cmd', self.on_motor_cmd, 10)
        self.create_timer(1.0 / fps, self.step)
        self.get_logger().info('virtual_world 시작 (장비 없이 가상 카메라·목표 발행)')

    def on_motor_cmd(self, msg):
        if len(msg.position) >= 2:
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
        return img

    def detect(self, img):
        mask = cv2.inRange(cv2.cvtColor(img, cv2.COLOR_BGR2HSV), BLUE_LOW, BLUE_HIGH)
        m = cv2.moments(mask, binaryImage=True)
        if m['m00'] < 20:
            return mask, None
        cx, cy = m['m10'] / m['m00'], m['m01'] / m['m00']
        w, h = self.width, self.height
        return mask, ((cx - w / 2) / (w / 2), (cy - h / 2) / (h / 2), m['m00'] / (w * h), cx, cy)

    def step(self):
        now = self.get_clock().now()
        t = (now - self.t0).nanoseconds * 1e-9
        img = self.render(t)
        mask, det = self.detect(img)
        stamp = now.to_msg()

        target = PointStamped()
        target.header.stamp = stamp
        target.header.frame_id = 'camera_color_optical_frame'
        if det:
            target.point.x, target.point.y, target.point.z = det[0], det[1], det[2]
        self.target_pub.publish(target)

        if self.publish_raw:
            raw = Image()
            raw.header = target.header
            raw.height, raw.width = img.shape[:2]
            raw.encoding = 'rgb8'
            raw.step = raw.width * 3
            raw.data = cv2.cvtColor(img, cv2.COLOR_BGR2RGB).tobytes()
            self.raw_pub.publish(raw)

        debug = img.copy()
        w, h = self.width, self.height
        cv2.drawMarker(debug, (w // 2, h // 2), (40, 40, 40), cv2.MARKER_CROSS, 30, 1)
        if det:
            cv2.circle(debug, (int(det[3]), int(det[4])), 8, (0, 0, 255), 2)
            label = f'DETECTED ex={det[0]:+.3f} ey={det[1]:+.3f}'
        else:
            label = 'NOT DETECTED'
        cv2.putText(debug, label, (10, 24), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (20, 20, 20), 2)
        cv2.putText(debug, f'pan={math.degrees(self.pan):+.1f} tilt={math.degrees(self.tilt):+.1f}',
                    (10, h - 12), cv2.FONT_HERSHEY_SIMPLEX, 0.55, (20, 20, 20), 1)
        for pub, frame in ((self.debug_pub, debug), (self.mask_pub, mask)):
            msg = CompressedImage()
            msg.header = target.header
            msg.format = 'jpeg'
            msg.data = cv2.imencode('.jpg', frame, [cv2.IMWRITE_JPEG_QUALITY, 70])[1].tobytes()
            pub.publish(msg)


def main(args=None):
    rclpy.init(args=args)
    try:
        rclpy.spin(VirtualWorld())
    except (KeyboardInterrupt, ExternalShutdownException):
        pass
    finally:
        rclpy.try_shutdown()
