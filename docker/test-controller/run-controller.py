"""테스트베드 중앙통제실 (호스트 Mac에서 실행)

  [로봇 카메라 영상]   │ [images 버튼 → 이미지] │ [status: 토픽별 message 최신값]
  [3D 월드 뷰]         │ [motors: 다이얼 2쌍]   │ [serial-out 실시간]
  [기둥 회전·배경]      │                       │ [토픽 버튼 → echo 실시간]

- 3D 시뮬레이션 (scene.py, pan_tilt.urdf):
    기구 URDF(pytransform3d)에 관절각을 넣어 카메라 자세를 구하고, 그 시점에서
    360° 배경(images/background/pano_*.jpg, 기본 격자 방) + 월드에 놓인 파란 기둥(30x30x60mm)을 렌더.
    관절각 = serial-out "M,Δpan,Δtilt" 누적 (펌웨어와 같은 계산) → 닫힌 루프:
      렌더 → debug/input-live/frame.png → fake_camera(live) → perception → /target → dynamixel
      → serial → 관절각 → 다시 렌더.  기둥을 옮기면 카메라가 따라 돌아 기둥이 가운데로 들어온다.
    (컨테이너: test-fake_camera_bringup 0)
  3D 월드 뷰: 왼쪽 드래그 = 기둥 바닥 이동 · Shift+드래그 = 높이 · Option(Alt)+드래그 = 기둥 회전(좌우 yaw, 위아래 pitch)
             오른쪽(또는 Ctrl) 드래그 = 시점 회전 · 휠 = 줌 · 슬라이더 = yaw·pitch·roll 직접 지정
             [색] = 선택한 상자 색 (perception HSV 범위(realsense.yaml) 안인지 옆에 표시)
  장애물(회색 벽판): 기본 2개, [장애물 추가]/[선택 삭제]. world 뷰에서 클릭해 선택 → 선택한 상자만 움직임
             먼 상자부터 그려 앞의 벽판이 기둥을 가림 → 벽 뒤에 숨기면 미검출 → LOST, 나오면 TRACKING
             [0으로 맞추기] = 기구 정면으로
- 이미지: debug/output-images/* (fake-camera.png = 지금 발행 중인 장면) +
          debug/topic/<토픽>/image.jpg (monitor_manager가 저장: 카메라·마스크·debug_image)
          버튼으로 골라 보고, 파일이 바뀌면 화면도 갱신
- status: debug/topic/<토픽>/message (monitor_manager가 모든 토픽의 마지막 메시지를 JSON으로 덮어씀).
          {"type", "data"} 중 data를 표시. 몇 초 전 값인지 표시, STALE_S 넘으면 빨강
- motors: 모터가 어디를 보고 있는지 다이얼로. 명령은 "변화량"이라 처음부터 누적해야 각도가 나옴 →
          · JointState 쌍: debug/topic/motor_cmd/message (monitor_manager가 메시지마다 덮어씀)를
                           0.05초마다 보고 header.stamp가 바뀌면 position[rad]을 누적
          · serial 쌍:     serial-out 의 "M,Δpan,Δtilt"[deg]를 처음부터 전부 누적 (펌웨어와 같은 계산)
          두 쌍이 같으면 dynamixel_controller의 rad→deg 변환·전송이 맞다는 뜻.
          JointState 쌍은 구독이 늦게 붙어 시작 직후(DDS 연결 전) 몇 개를 놓침 → serial 대비 누락 수 표시.
          [0으로 맞추기]를 누르면 두 쌍을 같은 시점부터 다시 누적 (그 뒤로는 같아야 정상).
          0° = 시작 위치 (펌웨어 기준 모터 180°). serial-out이 비워지면(새 실행) 두 쌍 모두 0으로 리셋.
          통제실을 켤 때 serial-out에 이미 있던 명령은 무시 (지금부터 들어오는 명령만 누적).
- serial-out: 모터 명령 등 가상 시리얼로 나간 값 (debug/serial-out tail)
- 토픽: debug/topic/<토픽>/echo 파일 tail. 컨테이너에서 `test-logger 0`이 돌고 있어야 계속 쌓임
컨테이너와는 공유 폴더(lv2_module5/debug)의 파일로만 주고받는다 (호스트에 ROS 불필요).

  cd docker/test-controller
  python3 -m venv .venv && .venv/bin/pip install -r requirements.txt   # 최초 1회
  .venv/bin/python run-controller.py
"""
import argparse
import fcntl
import json
import math
import os
import time
import re
import tkinter as tk
from tkinter import colorchooser
from pathlib import Path

import cv2
import numpy as np

import scene

DEBUG = Path(__file__).resolve().parents[2] / 'lv2_module5' / 'debug'
LIVE_IMAGE = DEBUG / 'input-live' / 'frame.png'   # fake_camera live 모드가 읽는 파일
LOCK = LIVE_IMAGE.with_name('.controller.lock')   # 통제실 하나만 (두 개면 frame.png를 번갈아 덮어씀)
BG_DIR = Path(__file__).resolve().parent / 'images' / 'background'
NO_BG = '(회색)'
PERCEPTION_YAML = DEBUG.parent / 'ros2_ws' / 'src' / 'realsense' / 'config' / 'realsense.yaml'
SERIAL_OUT = DEBUG / 'serial-out'
TOPIC_DIR = DEBUG / 'topic'
OUTPUT_DIR = DEBUG / 'output-images'
STALE_S = 2.0            # 이보다 오래 갱신 안 된 상태값은 빨강
MOTOR_MSG = TOPIC_DIR / 'motor_cmd' / 'message'
TARGET_LIMIT_DEG = (-180.0, 179.9)  # 펌웨어 config.h MIN/MAX_TARGET_DEG
DIAL = 120               # 다이얼 캔버스 크기(px)
DEFAULT_IMAGE = 'output-images/fake-camera.png'
TAIL_BYTES = 16 * 1024   # 처음 열 때·한 번에 밀려온 양이 클 때 보여줄 꼬리 크기
MAX_LINES = 1000         # 텍스트 창에 남길 최대 줄 수
PREVIEW_W = 480
VIEW_W, VIEW_H = 420, 300   # 로봇 카메라 미리보기 폭 · 3D 월드 뷰 크기
IMAGE_W = 320               # images 패널 폭


class Tail:
    """파일 끝에 붙는 내용을 Text 위젯에 계속 이어 붙인다 (tail -F). 파일이 비워지면 처음부터."""

    def __init__(self, text):
        self.text = text
        self.path = None
        self.pos = None

    def follow(self, path):
        self.path, self.pos = path, None
        self.text.delete('1.0', 'end')

    def poll(self):
        if self.path is None:
            return
        try:
            size = self.path.stat().st_size
        except FileNotFoundError:
            return
        if self.pos is None or size < self.pos:
            self.pos = 0
            self.text.delete('1.0', 'end')
        if size == self.pos:
            return
        self.pos = max(self.pos, size - TAIL_BYTES)
        with open(self.path, 'rb') as f:
            f.seek(self.pos)
            data = f.read(size - self.pos)
        self.pos = size
        at_bottom = self.text.yview()[1] >= 0.999
        self.text.insert('end', data.decode('utf-8', 'replace'))
        extra = int(self.text.index('end-1c').split('.')[0]) - MAX_LINES
        if extra > 0:
            self.text.delete('1.0', f'{extra + 1}.0')
        if at_bottom:
            self.text.see('end')


def to_photo(bgr, width=PREVIEW_W):
    """OpenCV BGR → Tk PhotoImage (폭 width로 축소). Tk가 PPM을 바로 읽어서 Pillow 불필요."""
    h, w = bgr.shape[:2]
    small = cv2.resize(bgr, (width, int(h * width / w)))
    return tk.PhotoImage(data=cv2.imencode('.ppm', small)[1].tobytes())


class ImageView:
    """이미지 파일이 바뀔 때마다(mtime) Label에 다시 그린다. 파일이 없으면 empty 문구."""

    def __init__(self, parent, empty, width=PREVIEW_W):
        self.label = tk.Label(parent, bg='black', fg='white')
        self.label.pack()
        self.info = tk.Label(parent, anchor='w')
        self.info.pack(fill='x')
        self.empty, self.width = empty, width
        self.follow(None)

    def follow(self, path):
        self.path, self.mtime, self.photo = path, None, None
        self.label.config(image='', text=self.empty)
        self.info.config(text='')

    def poll(self):
        try:
            mtime = self.path.stat().st_mtime
        except (AttributeError, FileNotFoundError):
            return
        if mtime == self.mtime:
            return
        img = cv2.imread(str(self.path))
        if img is not None:
            self.mtime = mtime
            self.photo = to_photo(img, self.width)
            self.label.config(image=self.photo, text='')
            self.info.config(text=f'{self.path.name}  갱신 {time.strftime("%H:%M:%S", time.localtime(mtime))}')


BTN = {'bg': '#e4e4e4', 'fg': 'black', 'font': ('', 12)}
BTN_SELECTED = {'bg': '#2e7d32', 'fg': 'white', 'font': ('', 12, 'bold')}


class NewLines:
    """파일에 새로 붙은 완전한 줄만 돌려준다. 파일이 비워지거나 새로 만들어지면 reset=True.
    처음 열 때 이미 있던 내용은 건너뜀 (지난 실행의 명령이 시뮬레이션 관절각에 섞이지 않게)."""

    def __init__(self, path):
        self.path, self.pos, self.ino, self.buf = path, 0, None, ''
        try:
            st = path.stat()
            self.pos, self.ino = st.st_size, st.st_ino
        except FileNotFoundError:
            pass

    def poll(self):
        try:
            st = self.path.stat()
        except FileNotFoundError:
            return False, []
        reset = st.st_ino != self.ino or st.st_size < self.pos
        if reset:
            self.ino, self.pos, self.buf = st.st_ino, 0, ''
        if st.st_size > self.pos:
            with open(self.path, 'rb') as f:
                f.seek(self.pos)
                self.buf += f.read(st.st_size - self.pos).decode('utf-8', 'replace')
            self.pos = st.st_size
        *lines, self.buf = self.buf.split('\n')
        return reset, lines


class MotorAccumulator:
    """변화량 명령을 누적해 pan·tilt 각도[deg]를 낸다 (펌웨어 apply_line과 같은 계산: 더하고 범위 제한)."""

    def __init__(self):
        self.reset()

    def reset(self):
        self.pan = self.tilt = 0.0
        self.last = None   # 마지막 (Δpan, Δtilt) [deg]
        self.count = 0

    def add(self, d_pan, d_tilt):
        lo, hi = TARGET_LIMIT_DEG
        self.pan = min(max(self.pan + d_pan, lo), hi)
        self.tilt = min(max(self.tilt + d_tilt, lo), hi)
        self.last = (d_pan, d_tilt)
        self.count += 1


def parse_serial(lines, acc):
    """serial-out 줄 중 "M,Δpan,Δtilt" [deg]만. 펌웨어 업로드 바이트 등 다른 줄은 무시."""
    for line in lines:
        parts = line.strip().split(',')
        if len(parts) == 3 and parts[0] == 'M':
            try:
                acc.add(float(parts[1]), float(parts[2]))
            except ValueError:
                pass


class JointStateMessage:
    """monitor_manager의 motor_cmd/message(JSON, 최신값)를 읽어 새 메시지(header.stamp 변화)면 누적."""

    def __init__(self, path, acc):
        self.path, self.acc, self.stamp = path, acc, None

    def poll(self):
        try:
            data = json.loads(self.path.read_text())['data']
        except (FileNotFoundError, ValueError, KeyError):
            return
        stamp = (data['header']['stamp']['sec'], data['header']['stamp']['nanosec'])
        if stamp == self.stamp:
            return
        first = self.stamp is None
        self.stamp = stamp
        if first:
            return   # 켜기 전에 이미 있던 값은 누적에 안 넣음 (시작점 = 지금)
        values = dict(zip(data.get('name', []), data.get('position', [])))
        if 'pan_joint' in values and 'tilt_joint' in values:
            self.acc.add(math.degrees(values['pan_joint']), math.degrees(values['tilt_joint']))


class Dial:
    """원형 다이얼: 위쪽 = 0°(시작 위치), 시계방향 = +. 바늘 = 누적 각도."""

    def __init__(self, parent, title):
        frame = tk.Frame(parent)
        frame.pack(side='left', padx=6)
        tk.Label(frame, text=title, font=('', 12, 'bold')).pack()
        self.canvas = tk.Canvas(frame, width=DIAL, height=DIAL, bg='white', highlightthickness=0)
        self.canvas.pack()
        self.text = tk.Label(frame, font=('Menlo', 11), width=22)
        self.text.pack()
        c, r = DIAL / 2, DIAL / 2 - 8
        self.c, self.r = c, r
        self.canvas.create_oval(c - r, c - r, c + r, c + r, outline='#999', width=2)
        for deg in range(0, 360, 45):   # 45° 눈금, 0°는 진하게
            x, y = self.point(deg, r), self.point(deg, r - (10 if deg == 0 else 5))
            self.canvas.create_line(*x, *y, fill='black' if deg == 0 else '#bbb', width=2)
        self.needle = self.canvas.create_line(c, c, c, c - r, fill='#2e7d32', width=4, arrow='last')

    def point(self, deg, radius):
        a = math.radians(deg)
        return self.c + radius * math.sin(a), self.c - radius * math.cos(a)

    def show(self, angle, delta):
        self.canvas.coords(self.needle, self.c, self.c, *self.point(angle, self.r - 6))
        d = '   -' if delta is None else f'{delta:+.2f}°'
        self.text.config(text=f'{angle:+7.2f}°  (Δ {d})')


class MotorPair:
    """pan·tilt 다이얼 한 쌍 + 누적기."""

    def __init__(self, parent, title, side='left'):
        frame = tk.LabelFrame(parent, text=title)
        frame.pack(side=side, fill='both', expand=True, padx=(0, 6))
        self.pan, self.tilt = Dial(frame, 'pan'), Dial(frame, 'tilt')
        self.info = tk.Label(frame, anchor='w', fg='#555')
        self.info.pack(side='bottom', fill='x')
        self.acc = MotorAccumulator()

    def draw(self, source_ok, note=''):
        last = self.acc.last or (None, None)
        self.pan.show(self.acc.pan, last[0])
        self.tilt.show(self.acc.tilt, last[1])
        self.info.config(text=f'명령 {self.acc.count}개 누적{note}' if source_ok else '파일 없음')


def hsv_ranges(path=PERCEPTION_YAML):
    """perception 검출 범위 [(lower, upper)] (OpenCV HSV)를 realsense.yaml에서 읽음 — 인지 팀이 바꾸면 따라감.
    ponytail: 정규식으로 키 몇 개만 읽음 (PyYAML 의존성 안 늘림). yaml 구조가 바뀌면 여기만 고치면 됨."""
    try:
        text = path.read_text()
    except FileNotFoundError:
        return []
    def vec(key):
        m = re.search(rf'^\s*{key}:\s*\[([^\]]*)\]', text, re.M)
        return tuple(int(v) for v in m.group(1).split(',')) if m else None
    out = [(vec('hsv_lower'), vec('hsv_upper'))]
    if re.search(r'^\s*hsv_bright_enabled:\s*true', text, re.M):
        out.append((vec('hsv_bright_lower'), vec('hsv_bright_upper')))
    return [r for r in out if None not in r]


def in_ranges(hsv, ranges):
    return any(all(lo <= c <= hi for c, lo, hi in zip(hsv, lower, upper)) for lower, upper in ranges)


def make_buttons(frame, names, command, cols, selected=None):
    """frame 안의 탭 버튼을 names로 다시 만든다 (cols열 격자).
    macOS tk.Button은 배경색을 무시해서 선택 강조가 안 보임 → 클릭 가능한 Label로 만든다."""
    for b in frame.winfo_children():
        b.destroy()
    for i, name in enumerate(names):
        b = tk.Label(frame, text=name, padx=6, pady=4, relief='raised', bd=1, cursor='hand2')
        b.bind('<Button-1>', lambda _e, n=name: command(n))
        b.grid(row=i // cols, column=i % cols, sticky='ew', padx=1, pady=1)
    highlight(frame, selected)


def highlight(frame, selected):
    """선택된 탭만 초록 배경·굵은 글씨, 나머지는 기본."""
    for b in frame.winfo_children():
        b.config(**(BTN_SELECTED if b.cget('text') == selected else BTN))


def text_box(parent, height):
    box = tk.Text(parent, height=height, width=52, font=('Menlo', 11),
                  bg='#111', fg='#9f9', insertbackground='#9f9')
    box.pack(fill='both', expand=True)
    return box


class Controller:
    def __init__(self, root):
        self.root = root
        self.frame = None
        self.robot = scene.Robot()
        self.env = scene.Environment()
        self.boxes = [scene.pillar()] + scene.default_obstacles()   # 0번 = 목표 기둥 (지울 수 없음)
        self.selected = self.boxes[0]   # 마우스·슬라이더·색이 적용될 상자 (world 뷰에서 클릭해 선택)
        self.observer = scene.Observer(width=VIEW_W, height=VIEW_H)
        self.joints = (0.0, 0.0)   # 렌더에 쓴 (pan, tilt) [deg] — 바뀌면 다시 그림
        self.scene_dirty = True
        self.drag = None
        LIVE_IMAGE.parent.mkdir(parents=True, exist_ok=True)

        root.title('Lv2 testbed control')
        sim, mid, right = tk.Frame(root), tk.Frame(root), tk.Frame(root)
        sim.pack(side='left', fill='y', padx=8, pady=8)
        mid.pack(side='left', fill='y', padx=(0, 8), pady=8)
        right.pack(side='left', fill='both', expand=True, padx=(0, 8), pady=8)

        # 1열 위: 로봇 카메라가 보는 영상 (= fake_camera live로 나가는 프레임)
        cam = tk.LabelFrame(sim, text='로봇 카메라 (camera_color_optical_frame)')
        cam.pack(fill='x')
        self.preview = tk.Label(cam, bg='black')
        self.preview.pack()

        # 1열 가운데: 3D 월드 뷰 (기구 URDF + 카메라 시야 + 기둥)
        world = tk.LabelFrame(sim, text='world  (클릭: 선택 · 드래그: 이동 · Shift: 높이 · Option/Alt: 회전 · 오른쪽/Ctrl: 시점 · 휠: 줌)')
        world.pack(fill='x', pady=(6, 0))
        self.view = tk.Canvas(world, width=VIEW_W, height=VIEW_H, bg='#20242a', highlightthickness=0)
        self.view.pack()
        for btn in (1, 2, 3):   # macOS 오른쪽 클릭 = Button-2, 다른 OS = Button-3
            self.view.bind(f'<ButtonPress-{btn}>', lambda e, b=btn: self.drag_start(e, b))
            self.view.bind(f'<B{btn}-Motion>', self.drag_move)
        self.view.bind('<MouseWheel>', self.wheel)
        # 기둥 회전: Option(macOS)·Alt(그 외)+왼쪽 드래그. 수정키 비트값이 OS마다 달라 이벤트 이름으로 묶음
        for mod in ('Option', 'Alt'):
            try:
                self.view.bind(f'<{mod}-ButtonPress-1>', lambda e: self.drag_start(e, 1, rotate=True))
            except tk.TclError:   # 그 OS에 없는 수정키 이름
                pass

        # 1열 아래: 기둥 회전·배경
        rot = tk.Frame(sim)
        rot.pack(fill='x')
        self.rot_vars = {}
        for name in ('yaw', 'pitch', 'roll'):
            var = tk.DoubleVar(value=0.0)
            tk.Scale(rot, label=name, variable=var, from_=-180, to=180, orient='horizontal',
                     length=VIEW_W // 3 - 6, command=lambda _v: self.set_rotation()).pack(side='left')
            self.rot_vars[name] = var
        objects = tk.Frame(sim)
        objects.pack(fill='x')
        self.selected_label = tk.Label(objects, font=('', 12, 'bold'), fg='#b8860b')
        self.selected_label.pack(side='left')
        tk.Button(objects, text='장애물 추가', command=self.add_obstacle).pack(side='left', padx=(8, 0))
        tk.Button(objects, text='선택 삭제', command=self.delete_selected).pack(side='left')
        tk.Button(objects, text='전체 초기화', command=self.reset_objects).pack(side='left', padx=(8, 0))
        controls = tk.Frame(sim)
        controls.pack(fill='x')
        tk.Label(controls, text='배경').pack(side='left')
        backgrounds = [NO_BG] + sorted(p.name for p in BG_DIR.glob('*')
                                       if p.suffix.lower() in ('.png', '.jpg', '.jpeg'))
        self.bg_choice = tk.StringVar(value=NO_BG)
        tk.OptionMenu(controls, self.bg_choice, *backgrounds, command=self.set_background).pack(side='left')
        tk.Button(controls, text='색', command=self.pick_color).pack(side='left', padx=(4, 0))
        self.swatch = tk.Label(controls, width=3, relief='sunken')   # Label은 macOS에서도 배경색이 보임
        self.swatch.pack(side='left')
        self.color_label = tk.Label(controls, font=('Menlo', 11))
        self.color_label.pack(side='left', padx=4)
        self.ranges = hsv_ranges()
        self.pose_label = tk.Label(sim, font=('Menlo', 11), anchor='w', justify='left')
        self.pose_label.pack(fill='x')
        self.status = tk.Label(sim, text=f'→ {LIVE_IMAGE}', wraplength=VIEW_W, justify='left', fg='#555')
        self.status.pack(fill='x')

        # 2열: 이미지 버튼 → 누른 이미지, 모터 다이얼 2쌍
        images = tk.LabelFrame(mid, text='images  (fake_camera_bringup 실행 중이어야 갱신)')
        images.pack(fill='x')
        self.image_buttons = tk.Frame(images)
        self.image_buttons.pack(fill='x')
        self.image_view = ImageView(images, '이미지 없음', width=IMAGE_W)
        self.image_names = []
        self.image_selected = None

        motors = tk.LabelFrame(mid, text='motors  (serial 누적 = 시뮬레이션 관절각)')
        motors.pack(fill='x', pady=(8, 0))
        self.joint_pair = MotorPair(motors, 'JointState  /motor_cmd position 누적', side='top')
        self.serial_pair = MotorPair(motors, 'serial-out  M,Δpan,Δtilt 누적', side='top')
        self.serial_lines = NewLines(SERIAL_OUT)
        self.joint_reader = JointStateMessage(MOTOR_MSG, self.joint_pair.acc)
        tk.Button(motors, text='0으로 맞추기 (기구 정면)', command=self.zero_motors).pack(fill='x')

        # 3열: 상태값, serial-out, 토픽 echo
        status = tk.LabelFrame(right, text='status  (debug/topic/<토픽>/message 최신값)')
        status.pack(fill='x')
        self.status_rows = tk.Frame(status)
        self.status_rows.pack(fill='x')
        self.status_names = []

        serial = tk.LabelFrame(right, text=f'serial-out  ({SERIAL_OUT})')
        serial.pack(fill='both', expand=True, pady=(8, 0))
        self.serial_tail = Tail(text_box(serial, 8))
        self.serial_tail.follow(SERIAL_OUT)

        topics = tk.LabelFrame(right, text='topics  (컨테이너에서 test-logger 0 실행 중이어야 갱신)')
        topics.pack(fill='both', expand=True, pady=(8, 0))
        self.buttons = tk.Frame(topics)
        self.buttons.pack(fill='x')
        self.topic_label = tk.Label(topics, text='토픽을 선택하세요', anchor='w')
        self.topic_label.pack(fill='x')
        self.topic_tail = Tail(text_box(topics, 14))
        self.topic_names = []
        self.topic_selected = None

        self.update_scene()
        self.update_images()
        self.update_status()
        self.update_motors()
        self.update_tails()
        self.update_topics()

    # ---- 3D 시뮬레이션 ----
    def pick(self, x, y, radius=45):
        """클릭 위치에서 가장 가까운 상자 (화면에서 중심까지 radius px 이내). 없으면 None"""
        uv, depth = self.observer.project([(b.x, b.y, b.z) for b in self.boxes])
        dist = np.hypot(uv[:, 0] - x, uv[:, 1] - y)
        dist[depth <= 0] = np.inf
        i = int(np.argmin(dist))
        return self.boxes[i] if dist[i] <= radius else None

    def select(self, box):
        self.selected = box
        for name, var in self.rot_vars.items():   # 슬라이더를 선택한 상자 각도로 (set은 command를 안 부름)
            var.set(round(getattr(box, name)))
        self.scene_dirty = True

    def drag_start(self, event, button, rotate=False):
        ctrl, shift = event.state & 0x4, event.state & 0x1
        if button == 1 and not ctrl:   # 왼쪽 클릭(수정키 무관) = 그 자리 상자 선택 후 조작
            hit = self.pick(event.x, event.y)
            if hit is not None and hit is not self.selected:
                self.select(hit)
        if rotate:
            mode = 'rotate'         # Option/Alt+드래그: 좌우 = yaw, 위아래 = pitch
        elif button != 1 or ctrl:
            mode = 'orbit'          # 오른쪽 드래그 (트랙패드는 Ctrl+드래그)
        else:
            mode = 'height' if shift else 'move'
        self.drag = (event.x, event.y, mode)

    def drag_move(self, event):
        if self.drag is None:
            return
        x0, y0, mode = self.drag
        du, dv = event.x - x0, event.y - y0
        if mode == 'move':
            dx, dy = self.observer.ground_move(du, dv)
            self.selected.x += dx
            self.selected.y += dy
        elif mode == 'height':
            self.selected.z = max(0.03, self.selected.z - dv * self.observer.dist / self.observer.f)
        elif mode == 'rotate':
            wrap = lambda a: (a + 180) % 360 - 180
            self.selected.yaw = wrap(self.selected.yaw - du * 0.5)      # 오른쪽으로 끌면 위에서 볼 때 시계방향
            self.selected.pitch = wrap(self.selected.pitch + dv * 0.5)  # 아래로 끌면 앞으로 숙임
            self.rot_vars['yaw'].set(round(self.selected.yaw))        # 슬라이더도 같이 (set은 command를 안 부름)
            self.rot_vars['pitch'].set(round(self.selected.pitch))
        else:
            self.observer.yaw -= du * 0.5
            self.observer.pitch = min(max(self.observer.pitch + dv * 0.5, -10), 89)
        self.drag = (event.x, event.y, mode)
        self.scene_dirty = True

    def wheel(self, event):
        self.observer.dist = min(max(self.observer.dist * 0.95 ** event.delta, 0.2), 4.0)
        self.scene_dirty = True

    def set_rotation(self):
        for name, var in self.rot_vars.items():
            setattr(self.selected, name, var.get())
        self.scene_dirty = True

    def pick_color(self):
        h, s_, v = self.selected.hsv
        bgr = cv2.cvtColor(np.uint8([[[h, s_, v]]]), cv2.COLOR_HSV2BGR)[0, 0]
        _, hex_color = colorchooser.askcolor(color='#%02x%02x%02x' % tuple(bgr[::-1]), title=f'{self.selected.name} 색')
        if hex_color:   # 취소하면 None
            r, g, b = (int(hex_color[i:i + 2], 16) for i in (1, 3, 5))
            self.selected.hsv = tuple(int(c) for c in cv2.cvtColor(np.uint8([[[b, g, r]]]), cv2.COLOR_BGR2HSV)[0, 0])
            self.scene_dirty = True

    def show_color(self):
        """견본 + HSV + perception 검출 범위 안인지 (가장 밝은 면 기준; 어두운 면은 V가 1/3까지 내려감)"""
        h, s_, v = self.selected.hsv
        b, g, r = cv2.cvtColor(np.uint8([[[h, s_, v]]]), cv2.COLOR_HSV2BGR)[0, 0]
        self.swatch.config(bg='#%02x%02x%02x' % (r, g, b))
        ok = in_ranges((h, s_, v), self.ranges)
        mark = '검출 범위 안 (잡힘)' if ok else '검출 범위 밖 (안 잡힘)'
        self.color_label.config(text=f'HSV({h},{s_},{v}) {mark}', fg='#2e7d32' if ok else '#c62828')

    def add_obstacle(self):
        n = len(self.boxes)   # 기둥 + 장애물 수 → 다음 번호
        box = scene.obstacle(0.3, 0.0, name=f'장애물 {n}')   # 기구 앞 0.3m (보이는 곳)
        self.boxes.append(box)
        self.select(box)

    def delete_selected(self):
        if self.selected is self.boxes[0]:
            self.status.config(text='기둥(목표)은 지울 수 없음')
            return
        self.boxes.remove(self.selected)
        self.select(self.boxes[0])

    def reset_objects(self):
        self.boxes = [scene.pillar()] + scene.default_obstacles()
        self.select(self.boxes[0])

    def set_background(self, name):
        image = None if name == NO_BG else cv2.imread(str(BG_DIR / name))
        self.env = scene.Environment(image)
        self.scene_dirty = True

    def draw_world(self):
        """3D 월드 뷰: 바닥 격자, world 축, URDF 상자, 카메라 시야, 기둥·장애물 (선택한 것은 노랑)"""
        c, obs = self.view, self.observer
        c.delete('all')

        def line(a, b, **kw):
            (p, q), d = obs.project([a, b])
            if d.min() > 0.01:
                c.create_line(*p, *q, **kw)

        for g in [i / 10 for i in range(-10, 11)]:   # 바닥 격자 10cm, ±1m
            line((g, -1, 0), (g, 1, 0), fill='#3a4048')
            line((-1, g, 0), (1, g, 0), fill='#3a4048')
        for axis, color in (((0.15, 0, 0), '#e05050'), ((0, 0.15, 0), '#50c050'), ((0, 0, 0.15), '#5080ff')):
            line((0, 0, 0), axis, fill=color, width=2)   # world x 빨강(정면)·y 초록·z 파랑
        for corners, link in self.robot.boxes():
            color = {'base_link': '#6a7cff', 'tilt_link': '#6a7cff', 'camera_link': '#d0d0d0'}.get(link, '#9a9a9a')
            for i, j in scene.BOX_EDGES:
                line(corners[i], corners[j], fill=color, width=2)
        cam = self.robot.camera()   # 카메라 시야 (0.6m까지)
        o, r = cam[:3, 3], cam[:3, :3]
        rays = [r @ np.array([(u - scene.CX) / scene.FX, (v - scene.CY) / scene.FY, 1]) * 0.6 + o
                for u, v in ((0, 0), (scene.W, 0), (scene.W, scene.H), (0, scene.H))]
        for k in range(4):
            line(o, rays[k], fill='#e0c040')
            line(rays[k], rays[(k + 1) % 4], fill='#e0c040')
        for box in self.boxes:
            selected = box is self.selected
            color = '#ffd040' if selected else ('#3080ff' if box is self.boxes[0] else '#a0a0a0')
            corners, _ = box.corners()
            for i, j in scene.BOX_EDGES:
                line(corners[i], corners[j], fill=color, width=3 if selected else 2)
        b = self.selected
        line((b.x, b.y, 0), (b.x, b.y, b.z - b.size[2] / 2), fill='#ffd040', dash=(2, 3))   # 바닥까지 점선 (높이 감)

    def update_scene(self):
        """기둥·시점·배경이 바뀌거나 관절각(serial 누적)이 바뀌면 다시 그려 live 파일 갱신."""
        joints = (self.serial_pair.acc.pan, self.serial_pair.acc.tilt)
        if self.scene_dirty or joints != self.joints:
            self.scene_dirty = False
            self.joints = joints
            self.robot.set(math.radians(joints[0]), math.radians(joints[1]))
            self.frame = scene.render(self.env, self.robot, self.boxes)
            self.photo = to_photo(self.frame, VIEW_W)
            self.preview.config(image=self.photo)
            # 임시 파일에 쓰고 교체 → fake_camera가 반쯤 쓰인 파일을 읽지 않게. 압축 1 = 빠르게
            tmp = LIVE_IMAGE.with_name('.tmp-' + LIVE_IMAGE.name)
            cv2.imwrite(str(tmp), self.frame, [cv2.IMWRITE_PNG_COMPRESSION, 1])
            os.replace(tmp, LIVE_IMAGE)
            self.draw_world()
            self.show_color()
            p = self.selected
            self.selected_label.config(text=f'선택: {p.name}')
            self.pose_label.config(text=f'{p.name} x {p.x:+.3f} y {p.y:+.3f} z {p.z:.3f} m   '
                                        f'관절 pan {joints[0]:+.1f}° tilt {joints[1]:+.1f}°')
        self.root.after(33, self.update_scene)

    def update_images(self):
        self.image_view.poll()
        self.root.after(300, self.update_images)

    def zero_motors(self):
        self.serial_pair.acc.reset()
        self.joint_pair.acc.reset()

    def update_motors(self):
        reset, lines = self.serial_lines.poll()
        if reset:  # 새 실행(serial-out 비워짐) → 두 쌍 모두 시작점부터
            self.serial_pair.acc.reset()
            self.joint_pair.acc.reset()
        parse_serial(lines, self.serial_pair.acc)
        self.joint_reader.poll()

        missed = self.serial_pair.acc.count - self.joint_pair.acc.count
        self.joint_pair.draw(MOTOR_MSG.exists(), f'  (serial 대비 누락 {missed})' if missed > 0 else '')
        self.serial_pair.draw(SERIAL_OUT.exists())
        self.root.after(50, self.update_motors)

    def update_status(self):
        files = sorted(TOPIC_DIR.rglob('message'))
        names = ['/' + str(p.parent.relative_to(TOPIC_DIR)) for p in files]
        if names != self.status_names:  # 파일이 생기거나 없어지면 줄을 다시 만든다
            self.status_names = names
            for w in self.status_rows.winfo_children():
                w.destroy()
            for i, name in enumerate(names):
                tk.Label(self.status_rows, text=name, font=('Menlo', 12, 'bold'), anchor='w'
                         ).grid(row=i, column=0, sticky='w', padx=(4, 8))
                tk.Label(self.status_rows, font=('Menlo', 12), width=8, anchor='e').grid(row=i, column=1)
                tk.Label(self.status_rows, font=('Menlo', 11), anchor='w', justify='left',
                         wraplength=380).grid(row=i, column=2, sticky='w', padx=8)
        cells = self.status_rows.winfo_children()
        for i, path in enumerate(files):
            try:
                age = time.time() - path.stat().st_mtime
                msg = json.loads(path.read_text())
            except (FileNotFoundError, ValueError):
                continue
            value = json.dumps(msg['data'], ensure_ascii=False, separators=(',', ':'))
            color = 'red' if age > STALE_S else '#2e7d32'
            cells[i * 3 + 1].config(text=f'{age:.1f}s', fg=color)
            cells[i * 3 + 2].config(text=value, fg='black' if age <= STALE_S else 'gray')
        self.root.after(300, self.update_status)

    @staticmethod
    def image_sources():
        """버튼 이름 → 파일. output-images의 이미지 + 토픽 폴더의 image.jpg (.tmp-* 임시 파일 제외)"""
        found = {f'output-images/{p.name}': p for p in sorted(OUTPUT_DIR.glob('*'))
                 if p.suffix in ('.png', '.jpg') and not p.name.startswith('.')}
        found.update({'/' + str(p.parent.relative_to(TOPIC_DIR)): p
                      for p in sorted(TOPIC_DIR.rglob('image.jpg'))})
        return found

    def update_tails(self):
        self.serial_tail.poll()
        self.topic_tail.poll()
        self.root.after(200, self.update_tails)

    def update_topics(self):
        # 토픽 echo 버튼
        names = sorted('/' + str(p.parent.relative_to(TOPIC_DIR)) for p in TOPIC_DIR.rglob('echo'))
        if names != self.topic_names:
            self.topic_names = names
            make_buttons(self.buttons, names, self.select_topic, cols=3, selected=self.topic_selected)
        # 이미지 버튼 (처음 생겼을 때 fake-camera.png를 기본으로 보여줌)
        self.images = self.image_sources()
        names = list(self.images)
        if names != self.image_names:
            self.image_names = names
            make_buttons(self.image_buttons, names, self.select_image, cols=2,
                         selected=self.image_selected)
            if self.image_selected is None and DEFAULT_IMAGE in self.images:
                self.select_image(DEFAULT_IMAGE)
        self.root.after(2000, self.update_topics)

    def select_image(self, name):
        self.image_selected = name
        highlight(self.image_buttons, name)
        self.image_view.follow(self.images[name])

    def select_topic(self, name):
        self.topic_selected = name
        highlight(self.buttons, name)
        self.topic_label.config(text=f'echo {name}')
        self.topic_tail.follow(TOPIC_DIR / name.lstrip('/') / 'echo')

    def close(self):
        self.root.destroy()


def single_instance():
    """통제실이 이미 떠 있으면 종료. 둘이면 서로 다른 장면을 frame.png에 번갈아 써서 카메라가 왔다 갔다 함.
    OS 잠금(flock)이라 비정상 종료돼도 자동으로 풀린다."""
    LOCK.parent.mkdir(parents=True, exist_ok=True)
    f = open(LOCK, 'a+')
    try:
        fcntl.flock(f, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        f.seek(0)
        raise SystemExit(f'통제실이 이미 실행 중입니다 (pid {f.read().strip() or "?"}). 그 창을 쓰거나 닫고 다시 실행하세요.')
    f.truncate(0)
    f.write(str(os.getpid()))
    f.flush()
    return f   # 프로세스가 살아있는 동안 잠금 유지


def main():
    argparse.ArgumentParser(description=__doc__.splitlines()[0]).parse_args()
    lock = single_instance()  # noqa: F841 (잠금 유지용)
    root = tk.Tk()
    app = Controller(root)
    root.protocol('WM_DELETE_WINDOW', app.close)
    root.mainloop()


if __name__ == '__main__':
    main()
