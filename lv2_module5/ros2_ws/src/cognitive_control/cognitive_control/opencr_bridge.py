"""OpenCR 자동 연결: /motor_cmd 를 OpenCR 시리얼 명령으로 보낸다.

포트(/dev/opencr, /dev/ttyACM*)가 생기면 열고, 쓰기에 실패하면(분리) 닫았다가 다시 찾는다.
명령 형식은 opencr_pan_tilt 펌웨어와 같다:  "M,<Δpan deg>,<Δtilt deg>\\n"  (115200 bps)
안전: 기본값으로 실제 카메라(/camera_source = realsense)일 때만 보낸다.
      가상 영상을 보고 낸 명령으로 실제 모터가 움직이지 않게 하기 위함.

구독  /motor_cmd (sensor_msgs/JointState, rad), /camera_source (std_msgs/String)
발행  /opencr_status (std_msgs/String)  "connected <포트>" | "disconnected"
      /joint_states (sensor_msgs/JointState)  보낸 명령을 누적한 pan/tilt 추정각 [rad] (3D 뷰용)
                    엔코더 측정값이 아니라 명령 누적이므로 펌웨어 한계각에서 잘린 만큼은 반영 안 됨
"""

import glob
import math
import os
import termios

import rclpy
from rclpy.executors import ExternalShutdownException
from rclpy.node import Node
from sensor_msgs.msg import JointState
from std_msgs.msg import String


def degrees_string(value):
    # 펌웨어 쪽 파서와 같은 형식: 소수 4자리, 뒤쪽 0 제거 (최소 한 자리는 남김)
    text = f'{value:.4f}'.rstrip('0')
    return text + '0' if text.endswith('.') else text


class OpenCRBridge(Node):

    def __init__(self):
        super().__init__('opencr_bridge')
        self.ports = list(self.declare_parameter(
            'ports', ['/dev/opencr', '/dev/ttyACM0', '/dev/ttyACM1']).value)
        self.require_real_camera = self.declare_parameter('require_real_camera', True).value
        self.fd = None
        self.port = None
        self.camera_source = 'virtual'
        self.status_pub = self.create_publisher(String, '/opencr_status', 10)
        self.joint_pub = self.create_publisher(JointState, '/joint_states', 10)
        self.pan = 0.0     # 연결 시점 = 펌웨어 기준 0°(정면)로 가정
        self.tilt = 0.0
        self.create_subscription(JointState, '/motor_cmd', self.on_motor_cmd, 10)
        self.create_subscription(String, '/camera_source', self.on_source, 10)
        self.create_timer(1.0, self.poll)

    def on_source(self, msg):
        self.camera_source = msg.data

    def candidates(self):
        found = [p for p in self.ports if os.path.exists(p)]
        return found + sorted(set(glob.glob('/dev/ttyACM*')) - set(found))

    def open_port(self, port):
        try:
            fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
            attr = termios.tcgetattr(fd)
            attr[0] = 0                                             # iflag
            attr[1] = 0                                             # oflag
            attr[2] = termios.CS8 | termios.CREAD | termios.CLOCAL  # cflag
            attr[3] = 0                                             # lflag
            attr[4] = attr[5] = termios.B115200
            termios.tcsetattr(fd, termios.TCSANOW, attr)
        except OSError as e:
            self.get_logger().warn(f'{port} 열기 실패: {e}', throttle_duration_sec=5.0)
            return False
        self.fd, self.port = fd, port
        self.pan = self.tilt = 0.0
        self.get_logger().info(f'OpenCR 연결: {port}')
        return True

    def close_port(self):
        if self.fd is not None:
            try:
                os.close(self.fd)
            except OSError:
                pass
            self.get_logger().info(f'OpenCR 분리: {self.port}')
        self.fd, self.port = None, None

    def poll(self):
        if self.fd is not None and not os.path.exists(self.port):
            self.close_port()
        if self.fd is None:
            for port in self.candidates():
                if self.open_port(port):
                    break
        status = f'connected {self.port}' if self.fd is not None else 'disconnected'
        self.status_pub.publish(String(data=status))

    def on_motor_cmd(self, msg):
        if self.fd is None or len(msg.position) < 2:
            return
        if self.require_real_camera and self.camera_source != 'realsense':
            return
        pan, tilt = (math.degrees(v) for v in msg.position[:2])
        line = f'M,{degrees_string(pan)},{degrees_string(tilt)}\n'.encode()
        try:
            os.write(self.fd, line)
        except OSError as e:
            self.get_logger().error(f'시리얼 쓰기 실패: {e}')
            self.close_port()
            return
        self.pan += msg.position[0]
        self.tilt += msg.position[1]
        joints = JointState()
        joints.header.stamp = self.get_clock().now().to_msg()
        joints.name = ['pan_joint', 'tilt_joint']
        joints.position = [self.pan, self.tilt]
        self.joint_pub.publish(joints)


def main(args=None):
    rclpy.init(args=args)
    node = OpenCRBridge()
    try:
        rclpy.spin(node)
    except (KeyboardInterrupt, ExternalShutdownException):
        pass
    finally:
        node.close_port()
        rclpy.try_shutdown()
