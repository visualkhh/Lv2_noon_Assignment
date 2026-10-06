"""인지: 카메라 영상(가상·RealSense 공통)에서 파란 목표를 찾아 /target 을 발행한다.

구독
  /camera/camera/color/image_raw  sensor_msgs/Image (rgb8 | bgr8)
발행 (실기 perception_node와 같은 이름·형식)
  /target   geometry_msgs/PointStamped  x,y = 화면 중심 정규화 오차 [-1,1], z = 면적 비율
            미검출이면 x=y=z=0
  /perception_node/debug_image/compressed   sensor_msgs/CompressedImage (jpeg)
  /perception_node/mask/compressed          sensor_msgs/CompressedImage (jpeg)
"""

import cv2
from geometry_msgs.msg import PointStamped
import numpy as np
import rclpy
from rclpy.executors import ExternalShutdownException
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy
from sensor_msgs.msg import CompressedImage, Image


class Perception(Node):

    def __init__(self):
        super().__init__('perception')
        # HSV 범위 (OpenCV: H 0~179). 실제 조명에서는 이 값을 맞춰야 한다
        self.low = np.array(self.declare_parameter('hsv_low', [100, 120, 60]).value)
        self.high = np.array(self.declare_parameter('hsv_high', [130, 255, 255]).value)
        self.min_area_ratio = self.declare_parameter('min_area_ratio', 0.0005).value
        self.max_area_ratio = self.declare_parameter('max_area_ratio', 0.5).value
        self.debug_period = self.declare_parameter('debug_period', 0.1).value  # 디버그 영상 간격(초)

        self.last_debug = 0.0
        self.target_pub = self.create_publisher(PointStamped, '/target', 10)
        self.debug_pub = self.create_publisher(
            CompressedImage, '/perception_node/debug_image/compressed', 10)
        self.mask_pub = self.create_publisher(
            CompressedImage, '/perception_node/mask/compressed', 10)
        # 영상은 메시지가 커서 best-effort면 조각 유실로 프레임이 자주 빠진다 → reliable
        # (realsense2_camera 기본 QoS도 reliable)
        reliable = self.declare_parameter('image_reliable', True).value
        qos = QoSProfile(depth=2, reliability=ReliabilityPolicy.RELIABLE if reliable
                         else ReliabilityPolicy.BEST_EFFORT)
        self.create_subscription(Image, '/camera/camera/color/image_raw', self.on_image, qos)

    def detect(self, bgr):
        mask = cv2.inRange(cv2.cvtColor(bgr, cv2.COLOR_BGR2HSV), self.low, self.high)
        mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, np.ones((3, 3), np.uint8))
        n, _, stats, centroids = cv2.connectedComponentsWithStats(mask)
        if n <= 1:
            return mask, None
        best = 1 + int(np.argmax(stats[1:, cv2.CC_STAT_AREA]))     # 가장 큰 덩어리 하나만
        h, w = mask.shape
        ratio = stats[best, cv2.CC_STAT_AREA] / (w * h)
        if not self.min_area_ratio <= ratio <= self.max_area_ratio:
            return mask, None
        cx, cy = centroids[best]
        return mask, ((cx - w / 2) / (w / 2), (cy - h / 2) / (h / 2), ratio, cx, cy,
                      stats[best, :4])

    def on_image(self, msg):
        if msg.encoding not in ('rgb8', 'bgr8'):
            self.get_logger().warn(f'지원 안 하는 encoding: {msg.encoding}',
                                   throttle_duration_sec=5.0)
            return
        img = np.frombuffer(msg.data, np.uint8).reshape(msg.height, msg.step)
        img = img[:, :msg.width * 3].reshape(msg.height, msg.width, 3)
        bgr = cv2.cvtColor(img, cv2.COLOR_RGB2BGR) if msg.encoding == 'rgb8' else img
        mask, det = self.detect(bgr)

        target = PointStamped()
        target.header = msg.header        # 원본 영상 시각 유지
        if det:
            target.point.x, target.point.y, target.point.z = det[0], det[1], det[2]
        self.target_pub.publish(target)

        now = self.get_clock().now().nanoseconds * 1e-9
        if now - self.last_debug < self.debug_period:
            return
        self.last_debug = now
        debug = bgr.copy()
        h, w = mask.shape
        cv2.drawMarker(debug, (w // 2, h // 2), (40, 40, 40), cv2.MARKER_CROSS, 30, 1)
        if det:
            x, y, bw, bh = (int(v) for v in det[5])
            cv2.rectangle(debug, (x, y), (x + bw, y + bh), (0, 0, 255), 2)
            label = f'DETECTED ex={det[0]:+.3f} ey={det[1]:+.3f}'
        else:
            label = 'NOT DETECTED'
        cv2.putText(debug, label, (10, 24), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (20, 20, 20), 2)
        for pub, frame in ((self.debug_pub, debug), (self.mask_pub, mask)):
            out = CompressedImage()
            out.header = msg.header
            out.format = 'jpeg'
            out.data = cv2.imencode('.jpg', frame, [cv2.IMWRITE_JPEG_QUALITY, 70])[1].tobytes()
            pub.publish(out)


def main(args=None):
    rclpy.init(args=args)
    try:
        rclpy.spin(Perception())
    except (KeyboardInterrupt, ExternalShutdownException):
        pass
    finally:
        rclpy.try_shutdown()
