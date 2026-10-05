"""3D 시뮬레이션: pan-tilt 기구(URDF) + 360° 배경 + 월드에 놓인 파란 사각 기둥.

  world 좌표: x 앞, y 왼쪽, z 위 [m] (URDF와 같음). 기구 베이스 = 원점.
  Robot:       pan_tilt.urdf를 pytransform3d(UrdfTransformManager)로 읽어 관절각 → 카메라 자세 (ROS tf처럼)
  Environment: equirectangular 360° 이미지(가로:세로 2:1)를 기구 주위 구(球)로 감쌈. 없으면 격자 방
  render():    카메라 광학 프레임에서 본 영상 = 시선마다 배경 샘플링 + 기둥 투영
  Observer:    통제실 3D 뷰용 관찰 시점 (궤도 회전·줌)

  Box:         월드에 놓인 상자. pillar() = 파란 목표 기둥, obstacle() = 회색 벽판(가림막)

  python3 scene.py   # 자체 점검 (assert)
"""
import math
from dataclasses import dataclass
from pathlib import Path

import cv2
import numpy as np
from pytransform3d.urdf import UrdfTransformManager

URDF = Path(__file__).resolve().parent / 'pan_tilt.urdf'
CAMERA_FRAME = 'camera_color_optical_frame'
W, H = 640, 480
# ponytail: RealSense D435 컬러 640x480의 대략적인 내부 파라미터. 실기 /camera_info 값으로 바꾸면 더 정확
FX = FY = 615.0
CX, CY = W / 2, H / 2
PILLAR = (0.030, 0.030, 0.060)   # world x·y·z 크기 [m] (세로가 긴 변, realsense.yaml의 목표 크기)
OBSTACLE = (0.020, 0.160, 0.220)  # 벽판: 두께·폭·높이 [m]. 카메라 높이(0.143m)보다 높아 뒤를 다 가림
OBSTACLE_HSV = (0, 0, 150)        # 회색 — 검출 범위 밖
# 기본 기둥 색 (OpenCV HSV): realsense.yaml 범위1 H 103~130, S ≥ 220 안. 가장 밝은 면 = 이 색
DEFAULT_HSV = (106, 250, 180)
DARK_RATIO = 1 / 3   # 조명을 등진 면의 밝기 비율 (밝은 면 대비)
TO_LIGHT = np.array([-0.6, 0.4, 0.7]) / np.linalg.norm([-0.6, 0.4, 0.7])  # 면 → 조명 (기구 쪽·왼쪽 위)

_CORNERS = np.array([[x, y, z] for x in (-1, 1) for y in (-1, 1) for z in (-1, 1)], float) / 2
_FACES = [   # (바깥 법선, 꼭짓점 4개 — 둘레 순서)
    ((1, 0, 0), (4, 5, 7, 6)), ((-1, 0, 0), (0, 2, 3, 1)),
    ((0, 1, 0), (2, 6, 7, 3)), ((0, -1, 0), (0, 1, 5, 4)),
    ((0, 0, 1), (1, 3, 7, 5)), ((0, 0, -1), (0, 4, 6, 2)),
]
BOX_EDGES = [(0, 1), (2, 3), (4, 5), (6, 7), (0, 2), (1, 3), (4, 6), (5, 7), (0, 4), (1, 5), (2, 6), (3, 7)]


# ---- 모양 메쉬: 크기 1(±0.5) 기준 (꼭짓점, [(바깥 법선, 면 꼭짓점 번호)], 와이어 선분) ----
# 세 모양 모두 볼록 → "카메라를 등진 면은 안 그림"만으로 한 물체 안의 가림이 맞음
def _box_mesh():
    return _CORNERS.copy(), [(np.array(n, float), list(i)) for n, i in _FACES], BOX_EDGES


def _cylinder_mesh(n=24):
    """세로(z)축 원기둥. 단면 지름 = 두께(x)·폭(y), 높이 = z"""
    a = np.linspace(0, 2 * np.pi, n, endpoint=False)
    ring = np.stack([np.cos(a), np.sin(a)], axis=1) / 2
    verts = np.vstack([np.c_[ring, np.full(n, -0.5)], np.c_[ring, np.full(n, 0.5)]])   # 0..n-1 아래, n.. 위
    faces = [(np.array([math.cos(a[i] + math.pi / n), math.sin(a[i] + math.pi / n), 0.0]),
              [i, (i + 1) % n, n + (i + 1) % n, n + i]) for i in range(n)]
    faces += [(np.array([0.0, 0.0, -1.0]), list(range(n))), (np.array([0.0, 0.0, 1.0]), list(range(n, 2 * n)))]
    edges = [(i, (i + 1) % n) for i in range(n)] + [(n + i, n + (i + 1) % n) for i in range(n)]
    edges += [(i, n + i) for i in range(0, n, n // 4)]
    return verts, faces, edges


def _sphere_mesh(n_lat=12, n_lon=24):
    """구 (크기가 다르면 타원체). 지름 = 두께(x)·폭(y)·높이(z)"""
    lat = np.linspace(-math.pi / 2, math.pi / 2, n_lat + 1)
    lon = np.linspace(0, 2 * math.pi, n_lon, endpoint=False)
    verts = np.array([[math.cos(t) * math.cos(p), math.cos(t) * math.sin(p), math.sin(t)]
                      for t in lat for p in lon]) / 2
    at = lambda i, j: i * n_lon + j % n_lon
    faces = []
    for i in range(n_lat):
        for j in range(n_lon):
            idx = [at(i, j), at(i, j + 1), at(i + 1, j + 1), at(i + 1, j)]   # 극 근처는 삼각형으로 겹침
            n = verts[idx].mean(axis=0)
            faces.append((n / np.linalg.norm(n), idx))
    eq = n_lat // 2
    edges = [(at(eq, j), at(eq, j + 1)) for j in range(n_lon)]                       # 적도
    edges += [(at(i, j), at(i + 1, j)) for j in (0, n_lon // 4, n_lon // 2, 3 * n_lon // 4) for i in range(n_lat)]
    return verts, faces, edges


SHAPES = {'box': '사각 기둥', 'cylinder': '원기둥', 'sphere': '구'}
MESHES = {'box': _box_mesh(), 'cylinder': _cylinder_mesh(), 'sphere': _sphere_mesh()}


def rot_zyx(yaw, pitch, roll):
    """world 기준 회전 [deg]: yaw(세로축 z) · pitch(y) · roll(x)"""
    y, p, r = (math.radians(a) for a in (yaw, pitch, roll))
    rz = np.array([[math.cos(y), -math.sin(y), 0], [math.sin(y), math.cos(y), 0], [0, 0, 1]])
    ry = np.array([[math.cos(p), 0, math.sin(p)], [0, 1, 0], [-math.sin(p), 0, math.cos(p)]])
    rx = np.array([[1, 0, 0], [0, math.cos(r), -math.sin(r)], [0, math.sin(r), math.cos(r)]])
    return rz @ ry @ rx


def box_corners(transform, size):
    """4x4 자세 + 크기 → 꼭짓점 8개 (world). 순서는 BOX_EDGES·_FACES와 맞음"""
    local = _CORNERS * np.asarray(size)
    return local @ transform[:3, :3].T + transform[:3, 3]


class Robot:
    def __init__(self, urdf=URDF):
        self.tm = UrdfTransformManager()
        self.tm.load_urdf(Path(urdf).read_text())
        self.set(0.0, 0.0)

    def set(self, pan, tilt):
        """관절각 [rad]"""
        self.pan, self.tilt = pan, tilt
        self.tm.set_joint('pan_joint', pan)
        self.tm.set_joint('tilt_joint', tilt)

    def camera(self):
        """camera_color_optical_frame → world 4x4"""
        return self.tm.get_transform(CAMERA_FRAME, 'world')

    def boxes(self):
        """URDF visual 상자들: (꼭짓점 8개 world, link 이름)"""
        out = []
        for v in self.tm.visuals:
            if hasattr(v, 'size'):
                out.append((box_corners(self.tm.get_transform(v.frame, 'world'), v.size),
                            v.frame.split(':')[1].split('/')[0]))
        return out


@dataclass
class Box:
    """월드에 놓인 물체: 중심 위치 [m] + 회전 [deg] + 크기(두께 x·폭 y·높이 z) + 색 + 모양(SHAPES)"""
    x: float = 0.5
    y: float = 0.0
    z: float = 0.143   # 기구 관절 0일 때 카메라 높이 → 기둥은 처음엔 화면 가운데
    yaw: float = 0.0
    pitch: float = 0.0
    roll: float = 0.0
    size: tuple = PILLAR
    hsv: tuple = DEFAULT_HSV   # OpenCV HSV (H 0~179). 면마다 V만 조명에 따라 줄어듦
    name: str = '기둥'
    shape: str = 'box'

    def geometry(self):
        """world 꼭짓점, [(world 법선, 면 꼭짓점 번호)], 와이어 선분"""
        rot = rot_zyx(self.yaw, self.pitch, self.roll)
        size = np.asarray(self.size, float)
        verts, faces, edges = MESHES[self.shape]
        verts_w = (verts * size) @ rot.T + (self.x, self.y, self.z)
        # 크기를 축마다 다르게 늘리면 법선은 크기로 나눠야 면에 수직으로 남음
        normals = [rot @ (n / size) for n, _ in faces]
        faces_w = [(nw / np.linalg.norm(nw), idx) for nw, (_, idx) in zip(normals, faces)]
        return verts_w, faces_w, edges


def pillar():
    return Box()


def obstacle(x, y, yaw=0.0, name='장애물'):
    """바닥에 세운 회색 벽판"""
    return Box(x=x, y=y, z=OBSTACLE[2] / 2, yaw=yaw, size=OBSTACLE, hsv=OBSTACLE_HSV, name=name)


def default_obstacles():
    """기둥(정면 0.5m) 뒤쪽 양옆(0.7m)에 벽판 2개 — 처음엔 기둥·배경이 다 보이고, 기둥을 벽 뒤로 밀면 숨음.
    (카메라 가까이 두면 0.35m에서 벽판 하나가 화면 절반을 가려 배경이 거의 안 보였음)"""
    return [obstacle(0.70, 0.18, name='장애물 1'), obstacle(0.70, -0.18, name='장애물 2')]


def grid_panorama(width=2048):
    """배경 이미지가 없을 때: 회색 격자 방 (경도 15°·위도 15° 선, 정면에 빨간 기준선). 파란색 없음."""
    h = width // 2
    lon = np.linspace(180, -180, width, endpoint=False)[None, :].repeat(h, 0)
    lat = np.linspace(90, -90, h)[:, None].repeat(width, 1)
    img = np.where(lat[..., None] < 0, 95, 150).astype(np.uint8).repeat(3, 2)   # 바닥 어둡게
    img[(np.abs((lon + 7.5) % 15 - 7.5) < 0.25) | (np.abs((lat + 7.5) % 15 - 7.5) < 0.25)] = 200
    img[np.abs(lat) < 0.4] = 230                                                  # 수평선
    img[np.abs(lon) < 0.6] = (40, 40, 220)                                        # 정면(world +x)
    return img


class Environment:
    """equirectangular 360° 배경. 가운데 = world +x(정면), 왼쪽으로 갈수록 +y, 위 = +z"""

    def __init__(self, image=None):
        self.image = grid_panorama() if image is None else image
        u, v = np.meshgrid(np.arange(W, dtype=np.float32), np.arange(H, dtype=np.float32))
        rays = np.stack([(u - CX) / FX, (v - CY) / FY, np.ones_like(u)], axis=-1)
        self.rays = (rays / np.linalg.norm(rays, axis=-1, keepdims=True)).reshape(-1, 3)

    def view(self, rotation):
        """카메라(광학 프레임) 회전 → W x H 배경"""
        d = self.rays @ rotation.T   # world 방향
        lon = np.arctan2(d[:, 1], d[:, 0])
        lat = np.arctan2(d[:, 2], np.hypot(d[:, 0], d[:, 1]))
        eh, ew = self.image.shape[:2]
        map_x = ((0.5 - lon / (2 * np.pi)) * ew).astype(np.float32).reshape(H, W)
        map_y = ((0.5 - lat / np.pi) * eh).astype(np.float32).reshape(H, W)
        return cv2.remap(self.image, map_x, map_y, cv2.INTER_LINEAR, borderMode=cv2.BORDER_WRAP)


def draw_box(frame, box, r_wc, t_wc):
    verts_w, faces, _ = box.geometry()
    verts = (verts_w - t_wc) @ r_wc   # world → 카메라 광학 프레임
    if np.any(verts[:, 2] <= 0.01):  # 카메라 뒤·바로 앞이면 안 그림
        return
    px = np.stack([FX * verts[:, 0] / verts[:, 2] + CX, FY * verts[:, 1] / verts[:, 2] + CY], axis=1)
    # 카메라를 등진 면은 안 그림 (볼록이라 한 물체 안에서는 면 순서 불필요)
    visible = [(n_w, idx) for n_w, idx in faces if (n_w @ r_wc) @ verts[idx].mean(axis=0) < 0]
    if not visible:
        return
    h, s_, v_max = box.hsv
    lit = np.array([max(0.0, n_w @ TO_LIGHT) for n_w, _ in visible])
    v = (v_max * (DARK_RATIO + (1 - DARK_RATIO) * lit)).astype(np.uint8)
    hsv = np.stack([np.full_like(v, h), np.full_like(v, s_), v], axis=1)[:, None, :]
    colors = cv2.cvtColor(hsv, cv2.COLOR_HSV2BGR)[:, 0, :].tolist()   # 면 색을 한 번에 변환
    for (_, idx), bgr in zip(visible, colors):
        cv2.fillConvexPoly(frame, np.round(px[idx]).astype(np.int32), bgr, cv2.LINE_AA)


def render(env, robot, boxes):
    """로봇 카메라가 보는 영상 (BGR W x H). 먼 상자부터 그려 가까운 상자가 가림.
    ponytail: 상자 중심 거리로 정렬하는 painter 방식 — 상자끼리 겹치거나 아주 긴 벽이 비스듬하면 틀릴 수 있음.
    정확히 하려면 픽셀별 깊이 버퍼(z-buffer)로."""
    cam = robot.camera()
    r_wc, t_wc = cam[:3, :3], cam[:3, 3]
    frame = env.view(r_wc)
    for box in sorted(boxes, key=lambda b: -np.linalg.norm(np.array([b.x, b.y, b.z]) - t_wc)):
        draw_box(frame, box, r_wc, t_wc)
    return frame


@dataclass
class Observer:
    """통제실 3D 뷰 시점: target을 중심으로 궤도 (yaw: 위에서 본 방향, pitch: 내려다보는 각) [deg]"""
    yaw: float = -140.0
    pitch: float = 25.0
    dist: float = 0.9
    target: tuple = (0.2, 0.0, 0.08)
    width: int = 420
    height: int = 300
    f: float = 420.0

    def basis(self):
        y, p = math.radians(self.yaw), math.radians(self.pitch)
        back = np.array([math.cos(p) * math.cos(y), math.cos(p) * math.sin(y), math.sin(p)])   # 관찰자 쪽
        eye = np.array(self.target) + self.dist * back
        fwd = -back
        right = np.cross(fwd, [0, 0, 1])
        right /= np.linalg.norm(right)
        up = np.cross(right, fwd)
        return eye, fwd, right, up

    def project(self, points):
        """world 점들 → (화면 좌표 N x 2, 깊이 N). 깊이 ≤ 0이면 관찰자 뒤"""
        eye, fwd, right, up = self.basis()
        rel = np.asarray(points) - eye
        depth = rel @ fwd
        safe = np.where(depth > 1e-3, depth, 1e-3)
        uv = np.stack([self.width / 2 + self.f * (rel @ right) / safe,
                       self.height / 2 - self.f * (rel @ up) / safe], axis=1)
        return uv, depth

    def ground_move(self, du, dv):
        """화면에서 du·dv 픽셀 끌면 바닥 평면에서 움직일 world (dx, dy)"""
        _, fwd, right, _ = self.basis()
        k = self.dist / self.f
        f2 = np.array([fwd[0], fwd[1]]) / (np.hypot(fwd[0], fwd[1]) or 1)
        r2 = np.array([right[0], right[1]])
        d = r2 * du * k - f2 * dv * k
        return float(d[0]), float(d[1])


def demo():
    robot, env = Robot(), Environment()

    def blob(frame):
        hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
        ys, xs = np.nonzero(cv2.inRange(hsv, (103, 220, 20), (130, 255, 255)))
        return xs, ys

    xs, ys = blob(render(env, robot, [pillar()]))
    assert abs(xs.mean() - CX) < 3 and abs(ys.mean() - CY) < 3, (xs.mean(), ys.mean())   # 처음엔 가운데
    assert 34 <= xs.max() - xs.min() + 1 <= 44, xs.max() - xs.min()                      # ≈ 615·0.03/0.48
    robot.set(math.radians(15), 0)        # 왼쪽으로 돌면 기둥은 화면 오른쪽
    assert blob(render(env, robot, [pillar()]))[0].mean() > CX + 100
    robot.set(0, math.radians(15))        # 아래를 보면 기둥은 화면 위쪽
    assert blob(render(env, robot, [pillar()]))[1].mean() < CY - 100
    robot.set(math.radians(180), 0)       # 뒤를 보면 기둥 없음
    assert len(blob(render(env, robot, [pillar()]))[0]) == 0
    robot.set(0, 0)                       # 정면 = 파노라마 가운데 (격자 방의 빨간 기준선)
    b, g, r = env.view(robot.camera()[:3, :3])[int(CY) - 60, int(CX)]
    assert r > 150 and b < 100, (b, g, r)
    assert len(robot.boxes()) == 6
    # 모양: 원기둥·구도 정면 0.5m에서 검출 범위 색으로 보이고, 구는 원(채움비 ≈ π/4)
    for shape in ('cylinder', 'sphere'):
        xs, ys = blob(render(env, robot, [Box(shape=shape, size=(0.04, 0.04, 0.04))]))
        assert len(xs) > 300 and abs(xs.mean() - CX) < 3, (shape, len(xs))
        fill = len(xs) / ((xs.max() - xs.min() + 1) * (ys.max() - ys.min() + 1))
        if shape == 'sphere':
            assert 0.72 < fill < 0.85, fill
    robot.set(0, 0)                       # 색 바꾸면 검출 범위 밖 (빨강) → 안 잡힘
    assert len(blob(render(env, robot, [Box(hsv=(0, 250, 200))]))[0]) == 0
    # 기둥 앞에 벽판을 두면 가려짐 (먼저 리스트에 있어도 거리순으로 그림)
    assert len(blob(render(env, robot, [obstacle(0.3, 0.0), pillar()]))[0]) == 0
    assert len(blob(render(env, robot, [pillar(), obstacle(0.7, 0.0)]))[0]) > 500   # 뒤에 있으면 안 가림
    # 기본 배치: 처음엔 기둥이 보이고, 벽판 뒤(0.85m, 좌 0.18m)로 밀면 숨음
    assert len(blob(render(env, robot, [pillar()] + default_obstacles()))[0]) > 500
    assert len(blob(render(env, robot, [Box(x=0.85, y=0.18)] + default_obstacles()))[0]) == 0
    uv, depth = Observer().project([[0, 0, 0]])
    assert depth[0] > 0
    print('scene OK')


if __name__ == '__main__':
    demo()
