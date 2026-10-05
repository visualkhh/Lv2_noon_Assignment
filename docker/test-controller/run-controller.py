"""테스트베드 중앙통제실 (호스트 Mac에서 실행)

  [웹캠 + 캡처 버튼]          │ [status: 토픽별 message 최신값]
                              │ [serial-out 실시간]
  [이미지 버튼들 → 누른 이미지] │ [토픽 버튼들 → 누른 토픽 echo 실시간]

- 캡처: 버튼(또는 스페이스)을 누를 때마다 현재 프레임을 <현재시각 ms>.png로 input-images에 저장
  → fake_camera_bringup의 fake_camera가 숫자 순서(=찍은 순서)대로 읽어 감
- 이미지: debug/output-images/* (fake-camera.png = 지금 발행 중인 장면) +
          debug/topic/<토픽>/image.jpg (monitor_manager가 저장: 카메라·마스크·debug_image)
          버튼으로 골라 보고, 파일이 바뀌면 화면도 갱신
- status: debug/topic/<토픽>/message (monitor_manager가 모든 토픽의 마지막 메시지를 JSON으로 덮어씀).
          {"type", "data"} 중 data를 표시. 몇 초 전 값인지 표시, STALE_S 넘으면 빨강
- serial-out: 모터 명령 등 가상 시리얼로 나간 값 (debug/serial-out tail)
- 토픽: debug/topic/<토픽>/echo 파일 tail. 컨테이너에서 `test-logger 0`이 돌고 있어야 계속 쌓임
컨테이너와는 공유 폴더(lv2_module5/debug)의 파일로만 주고받는다 (호스트에 ROS 불필요).

  cd docker/test-controller
  python3 -m venv .venv && .venv/bin/pip install -r requirements.txt   # 최초 1회
  .venv/bin/python run-controller.py [--camera 0]

macOS는 처음 실행 시 터미널에 카메라 권한을 허용해야 한다.
"""
import argparse
import json
import time
import tkinter as tk
from pathlib import Path

import cv2

DEBUG = Path(__file__).resolve().parents[2] / 'lv2_module5' / 'debug'
IMAGE_DIR = DEBUG / 'input-images'
SERIAL_OUT = DEBUG / 'serial-out'
TOPIC_DIR = DEBUG / 'topic'
OUTPUT_DIR = DEBUG / 'output-images'
STALE_S = 2.0            # 이보다 오래 갱신 안 된 상태값은 빨강
DEFAULT_IMAGE = 'output-images/fake-camera.png'
TAIL_BYTES = 16 * 1024   # 처음 열 때·한 번에 밀려온 양이 클 때 보여줄 꼬리 크기
MAX_LINES = 1000         # 텍스트 창에 남길 최대 줄 수
PREVIEW_W = 480


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
    box = tk.Text(parent, height=height, width=70, font=('Menlo', 11),
                  bg='#111', fg='#9f9', insertbackground='#9f9')
    box.pack(fill='both', expand=True)
    return box


class Controller:
    def __init__(self, root, camera):
        self.root = root
        self.cap = cv2.VideoCapture(camera)
        if not self.cap.isOpened():
            raise SystemExit(f'웹캠 {camera} 열기 실패 (카메라 권한 확인)')
        self.frame = None
        IMAGE_DIR.mkdir(parents=True, exist_ok=True)

        root.title('Lv2 testbed control')
        left = tk.Frame(root)
        left.pack(side='left', fill='y', padx=8, pady=8)
        right = tk.Frame(root)
        right.pack(side='left', fill='both', expand=True, padx=8, pady=8)

        # 왼쪽: 웹캠 + 캡처
        self.preview = tk.Label(left, bg='black')
        self.preview.pack()
        tk.Button(left, text='캡처 → input-images (space)', command=self.capture,
                  font=('', 14), pady=6).pack(fill='x', pady=6)
        self.status = tk.Label(left, text=f'저장 폴더: {IMAGE_DIR}', wraplength=PREVIEW_W, justify='left')
        self.status.pack(fill='x')
        root.bind('<space>', lambda _: self.capture())

        # 왼쪽 아래: 이미지 버튼 (output-images + 토픽 image.jpg) → 누른 이미지
        images = tk.LabelFrame(left, text='images  (fake_camera_bringup 실행 중이어야 갱신)')
        images.pack(fill='x', pady=(8, 0))
        self.image_buttons = tk.Frame(images)
        self.image_buttons.pack(fill='x')
        self.image_view = ImageView(images, '이미지 없음')
        self.image_names = []
        self.image_selected = None

        # 오른쪽 맨 위: 상태값 (debug/topic/<토픽>/message 최신 JSON)
        status = tk.LabelFrame(right, text='status  (debug/topic/<토픽>/message 최신값)')
        status.pack(fill='x')
        self.status_rows = tk.Frame(status)
        self.status_rows.pack(fill='x')
        self.status_names = []

        # 오른쪽 위: serial-out
        serial = tk.LabelFrame(right, text=f'serial-out  ({SERIAL_OUT})')
        serial.pack(fill='both', expand=True)
        self.serial_tail = Tail(text_box(serial, 12))
        self.serial_tail.follow(SERIAL_OUT)

        # 오른쪽 아래: 토픽 버튼 + echo
        topics = tk.LabelFrame(right, text='topics  (컨테이너에서 test-logger 0 실행 중이어야 갱신)')
        topics.pack(fill='both', expand=True, pady=(8, 0))
        self.buttons = tk.Frame(topics)
        self.buttons.pack(fill='x')
        self.topic_label = tk.Label(topics, text='토픽을 선택하세요', anchor='w')
        self.topic_label.pack(fill='x')
        self.topic_tail = Tail(text_box(topics, 18))
        self.topic_names = []
        self.topic_selected = None

        self.update_camera()
        self.update_images()
        self.update_status()
        self.update_tails()
        self.update_topics()

    def capture(self):
        if self.frame is None:
            return
        path = IMAGE_DIR / f'{int(time.time() * 1000)}.png'
        cv2.imwrite(str(path), self.frame)
        count = len(list(IMAGE_DIR.glob('*.png')))
        self.status.config(text=f'saved {path.name}  (input-images {count}장)')

    def update_camera(self):
        ok, frame = self.cap.read()
        if ok:
            self.frame = frame
            self.photo = to_photo(frame)
            self.preview.config(image=self.photo)
        self.root.after(33, self.update_camera)

    def update_images(self):
        self.image_view.poll()
        self.root.after(300, self.update_images)

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
                         wraplength=560).grid(row=i, column=2, sticky='w', padx=8)
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
        self.cap.release()
        self.root.destroy()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--camera', type=int, default=0, help='웹캠 번호')
    args = ap.parse_args()
    root = tk.Tk()
    app = Controller(root, args.camera)
    root.protocol('WM_DELETE_WINDOW', app.close)
    root.mainloop()


if __name__ == '__main__':
    main()
