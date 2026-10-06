"""가상환경용 추적 제어: /target → /motor_cmd, /tracking_status.

실기 제어 노드(dynamixel_move_node)와 같은 규칙·기본값을 쓴다.
  - z(면적 비율)=0이면 미검출 → 명령 없음, lost_timeout이 지나면 LOST
  - 데드밴드 밖이면 Δ = clamp(gain * 오차, ±max) 를 JointState(pan_joint, tilt_joint)로 발행
  - 상태 IDLE → TRACKING ↔ LOST, 바뀔 때와 status_publish_period마다 발행
"""

import math
import time

from geometry_msgs.msg import PointStamped
import rclpy
from rclpy.executors import ExternalShutdownException
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy
from sensor_msgs.msg import JointState
from std_msgs.msg import String


class Tracker(Node):

    def __init__(self):
        super().__init__('tracker')
        p = self.declare_parameter
        self.lost_timeout = p('lost_timeout', 0.5).value
        self.h_deadband = p('horizontal_deadband', 0.05).value
        self.v_deadband = p('vertical_deadband', 0.05).value
        self.pan_gain = p('pan_gain', -0.03).value
        self.tilt_gain = p('tilt_gain', 0.06).value
        self.max_pan = p('max_pan_command', 0.0872665).value
        self.max_tilt = p('max_tilt_command', 0.0872665).value
        period = p('status_publish_period', 1.0).value

        self.state = 'IDLE'
        self.last_valid = None
        self.motor_pub = self.create_publisher(JointState, '/motor_cmd', 10)
        self.status_pub = self.create_publisher(String, '/tracking_status', 10)
        qos = QoSProfile(depth=1, reliability=ReliabilityPolicy.BEST_EFFORT)
        self.create_subscription(PointStamped, '/target', self.on_target, qos)
        self.create_timer(period, self.publish_status)
        self.create_timer(0.1, self.check_timeout)

    def transition(self, state):
        if state != self.state:
            self.get_logger().info(f'{self.state} → {state}')
            self.state = state
            self.publish_status()

    def publish_status(self):
        self.status_pub.publish(String(data=self.state))

    def check_timeout(self):
        if (self.state == 'TRACKING' and self.last_valid is not None and
                time.monotonic() - self.last_valid >= self.lost_timeout):
            self.transition('LOST')

    def on_target(self, msg):
        x, y, z = msg.point.x, msg.point.y, msg.point.z
        if not math.isfinite(z) or z < 0.0 or z > 1.0 or z == 0.0:
            return
        if not (math.isfinite(x) and math.isfinite(y)) or abs(x) > 1.0 or abs(y) > 1.0:
            return
        self.last_valid = time.monotonic()
        self.transition('TRACKING')
        pan = 0.0 if abs(x) <= self.h_deadband else \
            max(-self.max_pan, min(self.max_pan, self.pan_gain * x))
        tilt = 0.0 if abs(y) <= self.v_deadband else \
            max(-self.max_tilt, min(self.max_tilt, self.tilt_gain * y))
        if pan == 0.0 and tilt == 0.0:
            return
        cmd = JointState()
        cmd.header.stamp = self.get_clock().now().to_msg()
        cmd.name = ['pan_joint', 'tilt_joint']
        cmd.position = [pan, tilt]
        self.motor_pub.publish(cmd)


def main(args=None):
    rclpy.init(args=args)
    try:
        rclpy.spin(Tracker())
    except (KeyboardInterrupt, ExternalShutdownException):
        pass
    finally:
        rclpy.try_shutdown()
