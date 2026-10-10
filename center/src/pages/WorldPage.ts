import { createElement, type CreateElementConfig, elementDefine, onConnectedBodyShadow, onConnectedAfter, onInitialize, onDisconnected, eventDelegateShadow, subscribeSwcAppMessage, appMessage, innerHtml, fetchManual, fetchSettled } from '@dooboostore/simple-web-component';
import type { SwcAppMessage } from '@dooboostore/simple-web-component';
import { inject } from '@dooboostore/simple-boot';
import { ValidUtils } from '@dooboostore/core-web';
import { WebSocketClient } from '@dooboostore/simple-boot-http-server/websocket/WebSocketClient';
import * as THREE from 'three';
import { ControlService } from '@app-src/services/ControlService';
import { TARGET_CHANGED, esc } from '@app-src/utils/ui';

export const tagName = 'world-page';

export interface WorldPage extends HTMLElement {}

export const WorldPage = (w: Window, data?: CreateElementConfig) => {
  return createElement<WorldPage>(w, tagName, data);
};

// scene.py 값과 맞춤: 기둥 30x30x60mm 파랑, 벽판 20x160x220mm 회색
const PILLAR_SIZE: [number, number, number] = [0.03, 0.03, 0.06];
const OBSTACLE_SIZE: [number, number, number] = [0.02, 0.16, 0.22];

// /api/center/urdf 파싱 결과 — joint 원점·visual 상자만 읽음 (rpy·limit·관성 등은 무시)
type UrdfModel = {
  panOrigin: [number, number, number];
  panAxis: [number, number, number];
  tiltOrigin: [number, number, number]; // pan 프레임 기준
  tiltAxis: [number, number, number];
  mountXyz: [number, number, number]; // camera_link 프레임 (tilt 기준)
  opticalXyz: [number, number, number]; // optical 프레임 (camera_link 기준)
  visuals: { link: string; xyz: [number, number, number]; size: [number, number, number] }[];
};

const num3 = (s: string | null, fallback: [number, number, number]): [number, number, number] => {
  const v = (s ?? '').trim().split(/\s+/).map(Number);
  return v.length === 3 && v.every(Number.isFinite) ? [v[0], v[1], v[2]] : fallback;
};

const parseUrdf = (xml: string): UrdfModel | null => {
  try {
    const doc = new DOMParser().parseFromString(xml, 'application/xml');
    if (doc.querySelector('parsererror')) return null;
    const joint = (name: string) => [...doc.querySelectorAll('joint')].find((j) => j.getAttribute('name') === name);
    const origin = (el: Element | undefined | null, fb: [number, number, number]) =>
      num3(el?.querySelector('origin')?.getAttribute('xyz') ?? null, fb);
    const axis = (el: Element | undefined | null, fb: [number, number, number]) =>
      num3(el?.querySelector('axis')?.getAttribute('xyz') ?? null, fb);
    const visuals: UrdfModel['visuals'] = [];
    for (const link of doc.querySelectorAll('robot > link')) {
      const name = link.getAttribute('name') ?? '';
      for (const v of link.querySelectorAll('visual')) {
        const box = v.querySelector('geometry > box')?.getAttribute('size');
        if (!box) continue; // box visual만 그림
        visuals.push({ link: name, xyz: origin(v, [0, 0, 0]), size: num3(box, [0.01, 0.01, 0.01]) });
      }
    }
    return {
      panOrigin: origin(joint('pan_joint'), [0, 0, 0.0515]),
      panAxis: axis(joint('pan_joint'), [0, 0, 1]),
      tiltOrigin: origin(joint('tilt_joint'), [0, 0, 0.04]),
      tiltAxis: axis(joint('tilt_joint'), [0, 1, 0]),
      mountXyz: origin(joint('camera_mount'), [0.01, 0, 0.052]),
      opticalXyz: origin(joint('camera_optical'), [0.0125, 0, 0]),
      visuals
    };
  } catch {
    return null;
  }
};

export const defineWorldPage = async (w: Window) => {
  const existing = w.customElements.get(tagName);
  if (existing) return existing;

  @elementDefine(tagName, { window: w })
  class WorldPageImp extends (w.HTMLElement as unknown as typeof HTMLElement) implements WorldPage {
    private controlService?: ControlService;
    private target = '';
    private streaming = false;
    private sending = false;
    private sock?: WebSocketClient;
    private statusTimer?: ReturnType<typeof setInterval>;
    private frameTimer?: ReturnType<typeof setInterval>;
    private raf = 0;
    private renderer?: THREE.WebGLRenderer;
    private robotRenderer?: THREE.WebGLRenderer;
    private scene?: THREE.Scene;
    private camera?: THREE.PerspectiveCamera;
    private robotCamera?: THREE.PerspectiveCamera;
    private rig?: THREE.Group; // pan(yaw) → tilt(pitch) 기구
    private pillar?: THREE.Mesh;
    private frustumCam?: THREE.PerspectiveCamera;
    private frustumHelper?: THREE.CameraHelper;
    private pan = 0;
    private tilt = 0;
    private panAxisProp: 'z' | 'y' = 'z';
    private tiltAxisProp: 'y' | 'z' = 'y';
    private obsYaw = -140;
    private obsPitch = 25;
    private obsDist = 0.9;
    private obsTarget: [number, number, number] = [0.2, 0, 0.08];

    private applyObserver() {
      if (!this.camera) return;
      const y = THREE.MathUtils.degToRad(this.obsYaw);
      const p = THREE.MathUtils.degToRad(this.obsPitch);
      const back: [number, number, number] = [Math.cos(p) * Math.cos(y), Math.cos(p) * Math.sin(y), Math.sin(p)];
      this.camera.position.set(
        this.obsTarget[0] + this.obsDist * back[0],
        this.obsTarget[1] + this.obsDist * back[1],
        this.obsTarget[2] + this.obsDist * back[2]
      );
      this.camera.lookAt(this.obsTarget[0], this.obsTarget[1], this.obsTarget[2]);
    }

    private showPillarPos(root: ShadowRoot) {
      const p = this.pillar?.position;
      const pos = root.querySelector('.pillar-pos');
      if (p && pos) {
        const f = (v: number) => `${v >= 0 ? '+' : ''}${v.toFixed(3)}`;
        pos.textContent = `기둥 x ${f(p.x)} y ${f(p.y)} z ${f(p.z)} m`;
      }
    }

    @onInitialize
    init(@inject(ControlService.SYMBOL) controlService: ControlService) {
      this.controlService = controlService;
    }

    @subscribeSwcAppMessage(TARGET_CHANGED, { subject: 'behavior' })
    onTarget(@appMessage msg: SwcAppMessage<string>) {
      if (msg?.data) this.target = msg.data;
    }

    @onConnectedAfter
    async start() {
      if (!ValidUtils.isBrowser()) return; // SSR에선 three.js 초기화 안 함
      try {
        const { targets } = await this.controlService!.listTargets({});
        if (targets.length && !this.target) this.target = targets[0].id;
      } catch {
        /* 무시 */
      }
      this.initScene();
      this.statusTimer = setInterval(() => void this.pollMotors(), 500);
    }

    @onDisconnected
    stop() {
      if (this.statusTimer) clearInterval(this.statusTimer);
      if (this.frameTimer) clearInterval(this.frameTimer);
      cancelAnimationFrame(this.raf);
      this.closeSocket();
      this.statusTimer = this.frameTimer = undefined;
      this.renderer?.dispose();
      this.robotRenderer?.dispose();
      this.renderer = this.robotRenderer = undefined;
    }

    private async pollMotors() {
      if (!this.controlService || !this.target) return;
      try {
        const m = await this.controlService.getMotors({ target: this.target });
        // serial(펌웨어가 받은 값) 우선, 없으면 JointState — run-controller.py joint_source와 같음
        const src = m.serial.count > 0 ? m.serial : m.joint;
        this.pan = src.pan;
        this.tilt = src.tilt;
        const label = this.shadowRoot?.querySelector('.joints');
        if (label) label.textContent = `관절 pan ${this.pan >= 0 ? '+' : ''}${this.pan.toFixed(1)}° tilt ${this.tilt >= 0 ? '+' : ''}${this.tilt.toFixed(1)}° (출처: ${m.source})`;
      } catch {
        /* 무시 */
      }
    }

    private initScene() {
      void this.initSceneAsync();
    }

    private async initSceneAsync() {
      const root = this.shadowRoot;
      if (!root) return;
      const worldCanvas = root.querySelector('.world') as HTMLCanvasElement | null;
      const robotCanvas = root.querySelector('.robot') as HTMLCanvasElement | null;
      if (!worldCanvas || !robotCanvas) return;
      // URDF 원문 수신 → 파싱 실패하면 하드코딩(현 pan_tilt.urdf 값)으로 폴백
      let urdf: UrdfModel | null = null;
      try {
        const res = await fetch('/api/center/urdf');
        if (res.ok) urdf = parseUrdf(await res.text());
      } catch {
        /* 폴백 */
      }
      const note = root.querySelector('.urdf-note');
      if (note) note.textContent = urdf ? `URDF ${urdf.visuals.length}개 visual (서버 수신)` : 'URDF 수신 실패 — 내장 치수로 표시';

      this.scene = new THREE.Scene();
      this.scene.background = new THREE.Color(0x20242a);
      // 바닥 격자: GridHelper는 XZ평면(y-up 기준)이라 z-up 월드에선 세로 커튼이 됨 → 90° 눕힘
      const grid = new THREE.GridHelper(2, 20, 0x3a4048, 0x3a4048);
      grid.rotation.x = Math.PI / 2;
      this.scene.add(grid);
      this.scene.add(new THREE.AmbientLight(0xffffff, 0.9));
      const light = new THREE.DirectionalLight(0xffffff, 1.2);
      light.position.set(-0.6, 0.4, 0.7); // scene.py TO_LIGHT와 같음
      this.scene.add(light);

      // 기구 (URDF 파싱 결과로 조립 — 축이 z가 아니면 y로 폴백):
      //   panGroup(pan 원점) → pan_link visuals + tiltGroup(tilt 원점) → tilt/camera visuals + 카메라
      const panAxisZ = Math.abs(urdf?.panAxis[2] ?? 1) >= 0.9;
      const tiltAxisY = Math.abs(urdf?.tiltAxis[1] ?? 1) >= 0.9;
      this.panAxisProp = panAxisZ ? 'z' : 'y';
      this.tiltAxisProp = tiltAxisY ? 'y' : 'z';
      this.rig = new THREE.Group();
      this.rig.position.set(...(urdf?.panOrigin ?? [0, 0, 0.0515]));
      const tiltG = new THREE.Group();
      tiltG.position.set(...(urdf?.tiltOrigin ?? [0, 0, 0.04]));
      const mount: [number, number, number] = urdf?.mountXyz ?? [0.01, 0, 0.052];
      const optical: [number, number, number] = urdf?.opticalXyz ?? [0.0125, 0, 0];
      this.robotCamera = new THREE.PerspectiveCamera(43, 640 / 480, 0.01, 10);
      this.robotCamera.up.set(0, 0, 1); // world는 z-up (기본 +y 그대로면 화면이 뒤집힘)
      this.robotCamera.position.set(mount[0] + optical[0], mount[1] + optical[1], mount[2] + optical[2]);
      tiltG.add(this.robotCamera);
      this.rig.add(tiltG);
      this.rig.userData.tilt = tiltG;
      this.scene.add(this.rig);
      const matBody = new THREE.MeshStandardMaterial({ color: 0x6a7cff });
      const matCam = new THREE.MeshStandardMaterial({ color: 0xd0d0d0 });
      const part = (parent: THREE.Object3D, xyz: [number, number, number], size: [number, number, number], m = matBody) => {
        const mesh = new THREE.Mesh(new THREE.BoxGeometry(...size), m);
        mesh.position.set(...xyz);
        parent.add(mesh);
      };
      if (urdf && urdf.visuals.length) {
        for (const v of urdf.visuals) {
          if (v.link === 'base_link' || v.link === 'world') part(this.scene!, v.xyz, v.size);
          else if (v.link === 'pan_link') part(this.rig, v.xyz, v.size);
          else if (v.link === 'tilt_link') part(tiltG, v.xyz, v.size);
          else if (v.link === 'camera_link')
            part(tiltG, [mount[0] + v.xyz[0], mount[1] + v.xyz[1], mount[2] + v.xyz[2]], v.size, matCam);
        }
      } else {
        // 폴백 (현 pan_tilt.urdf 값)
        part(this.scene, [0, 0, 0.0025], [0.12, 0.12, 0.005]);
        part(this.scene, [0, 0, 0.02825], [0.034, 0.0285, 0.0465]);
        part(this.rig, [0, 0, 0.006], [0.04, 0.034, 0.012]);
        part(this.rig, [0, 0, 0.04], [0.034, 0.0465, 0.0285]);
        part(tiltG, [0, 0, 0.026], [0.022, 0.03, 0.028]);
        part(tiltG, mount, [0.025, 0.09, 0.025], matCam);
      }
      this.scene.updateMatrixWorld(true);
      this.robotCamera.lookAt(new THREE.Vector3(1, 0, mount[2] + optical[2])); // 정면 수평 (world 좌표)
      // 카메라 시야 원뿔 (scene.py 노랑 시야선相当 — 0.6m까지). 렌더용 카메라(far 10m)와 분리:
      // helper가 far까지 그리면 10m짜리 원뿔이 돼서 표시용 카메라(far 0.6m)를 두고 자세만 복사
      this.frustumCam = new THREE.PerspectiveCamera(43, 640 / 480, 0.01, 0.6);
      this.frustumHelper = new THREE.CameraHelper(this.frustumCam);
      (this.frustumHelper.material as THREE.LineBasicMaterial).color.set(0xe0c040);
      this.scene.add(this.frustumHelper);
      // 기구 외형은 위 URDF 조립에서 끝 (삭제된 구 part() 블록 자리)

      // 목표 기둥 (파랑) + 장애물 2개 (회색) — scene.py default_obstacles와 같은 배치
      this.pillar = new THREE.Mesh(
        new THREE.BoxGeometry(...PILLAR_SIZE),
        new THREE.MeshStandardMaterial({ color: 0x3080ff })
      );
      this.pillar.position.set(0.5, 0, 0.143);
      this.pillar.userData.name = '기둥';
      this.scene.add(this.pillar);
      for (const [x, y, name] of [[0.7, 0.18, '장애물 1'], [0.7, -0.18, '장애물 2']] as const) {
        const ob = new THREE.Mesh(new THREE.BoxGeometry(...OBSTACLE_SIZE), new THREE.MeshStandardMaterial({ color: 0xa0a0a0 }));
        ob.position.set(x, y, OBSTACLE_SIZE[2] / 2);
        ob.userData.name = name;
        this.scene.add(ob);
      }

      this.camera = new THREE.PerspectiveCamera(50, 420 / 300, 0.01, 20);
      this.camera.up.set(0, 0, 1);
      // 관찰 시점 (scene.py Observer와 같음: yaw=위에서 본 방향, pitch=내려다보는 각)
      this.obsYaw = -140;
      this.obsPitch = 25;
      this.obsDist = 0.9;
      this.obsTarget = [0.2, 0, 0.08];
      this.applyObserver();

      this.renderer = new THREE.WebGLRenderer({ canvas: worldCanvas, antialias: true });
      this.renderer.setSize(420, 300, false);
      this.robotRenderer = new THREE.WebGLRenderer({ canvas: robotCanvas, antialias: true, preserveDrawingBuffer: true });
      this.robotRenderer.setSize(640, 480, false);

      // 드래그 = 기둥 바닥 이동 · Shift+드래그 = 높이 · 오른쪽/Ctrl+드래그 = 시점 회전 · 휠 = 줌
      // (run-controller.py world 뷰와 같음)
      const ray = new THREE.Raycaster();
      const ground = new THREE.Plane(new THREE.Vector3(0, 0, 1), 0);
      let drag: { x: number; y: number; mode: 'move' | 'height' | 'orbit' } | null = null;
      worldCanvas.addEventListener('contextmenu', (e) => e.preventDefault());
      worldCanvas.addEventListener('pointerdown', (e) => {
        worldCanvas.setPointerCapture(e.pointerId);
        drag = { x: e.clientX, y: e.clientY, mode: e.button === 2 || e.ctrlKey ? 'orbit' : e.shiftKey ? 'height' : 'move' };
      });
      worldCanvas.addEventListener('pointerup', () => {
        drag = null;
      });
      worldCanvas.addEventListener('pointermove', (e) => {
        if (!drag || !this.pillar || !this.camera) return;
        const du = e.clientX - drag.x;
        const dv = e.clientY - drag.y;
        drag = { ...drag, x: e.clientX, y: e.clientY };
        if (drag.mode === 'orbit') {
          this.obsYaw -= du * 0.5;
          this.obsPitch = Math.min(Math.max(this.obsPitch + dv * 0.5, -10), 89);
          this.applyObserver();
        } else if (drag.mode === 'height') {
          const p = this.pillar.position;
          p.z = Math.min(Math.max(p.z - (dv * this.obsDist) / 420, 0.03), 1.0);
          this.showPillarPos(root);
        } else {
          const r = worldCanvas.getBoundingClientRect();
          const ndc = new THREE.Vector2(((e.clientX - r.left) / r.width) * 2 - 1, -((e.clientY - r.top) / r.height) * 2 + 1);
          ray.setFromCamera(ndc, this.camera);
          const hit = new THREE.Vector3();
          if (ray.ray.intersectPlane(ground, hit)) {
            this.pillar.position.x = hit.x;
            this.pillar.position.y = hit.y;
            this.showPillarPos(root);
          }
        }
      });
      worldCanvas.addEventListener(
        'wheel',
        (e) => {
          e.preventDefault();
          this.obsDist = Math.min(Math.max(this.obsDist * (e.deltaY > 0 ? 1.1 : 0.9), 0.2), 4.0);
          this.applyObserver();
        },
        { passive: false }
      );

      const loop = () => {
        this.raf = requestAnimationFrame(loop);
        if (!this.rig || !this.scene || !this.camera || !this.renderer || !this.robotRenderer || !this.robotCamera) return;
        this.rig.rotation[this.panAxisProp] = THREE.MathUtils.degToRad(this.pan);
        this.rig.userData.tilt.rotation[this.tiltAxisProp] = THREE.MathUtils.degToRad(this.tilt); // URDF: tilt+ = 아래
        if (this.frustumCam && this.frustumHelper) {
          this.robotCamera.getWorldPosition(this.frustumCam.position);
          this.robotCamera.getWorldQuaternion(this.frustumCam.quaternion);
          // frustumCam은 scene에 안 붙은 고아 노드라 renderer가 matrixWorld를 안 갱신함.
          // 복사만 하고 update를 안 하면 단위행렬(= three -Z = world -z, 정 아래)로 그려짐
          this.frustumCam.updateMatrixWorld();
          this.frustumHelper.update();
        }
        this.renderer.render(this.scene, this.camera);
        this.robotRenderer.render(this.scene, this.robotCamera);
      };
      loop();
    }

    @eventDelegateShadow('.stream-toggle', 'click')
    async onStreamToggle() {
      if (!this.controlService || !this.target) return;
      this.streaming = !this.streaming;
      const btn = this.shadowRoot?.querySelector('.stream-toggle');
      const label = this.shadowRoot?.querySelector('.stream-state');
      if (this.streaming) {
        // ws 바이너리 송출 (10fps) — canvas → JPEG → /api/center/frames → frame.png → fake_camera
        this.openSocket();
        this.frameTimer = setInterval(() => void this.sendFrame(), 100);
        if (btn) {
          btn.textContent = '● 송출 중';
          btn.classList.add('on');
        }
        if (label) label.textContent = 'three.js → frame.png → fake_camera';
      } else {
        if (this.frameTimer) clearInterval(this.frameTimer);
        this.frameTimer = undefined;
        this.closeSocket();
        await this.controlService.setStreaming({ target: this.target, on: false });
        if (btn) {
          btn.textContent = '■ 송출';
          btn.classList.remove('on');
        }
        if (label) label.textContent = '꺼짐 (카메라 없음)';
      }
    }

    private openSocket() {
      this.closeSocket();
      if (!this.target) return;
      try {
        const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
        // 끊기면 5회까지 1초 간격 재접속. 대기열은 최신 1장만 — 묵은 프레임이 쏟아지지 않게
        this.sock = new WebSocketClient(`${proto}//${location.host}/api/center/frames?target=${encodeURIComponent(this.target)}`, {
          retryConnectionCount: 5,
          retryConnectionDelay: 1000,
          maxPendingBinaryMessages: 1
        });
      } catch {
        this.sock = undefined;
      }
    }

    private closeSocket() {
      try {
        this.sock?.close();
      } catch {
        /* 무시 */
      }
      this.sock = undefined;
    }

    @eventDelegateShadow('.zero-motors', 'click')
    async onZeroMotors() {
      if (!this.controlService || !this.target) return;
      await this.controlService.resetMotors({ target: this.target });
    }

    private async sendFrame() {
      // 이전 전송이 끝나기 전이면 스킵 — 겹쳐 쌓이면 체감 지연만 커짐
      if (!this.robotRenderer || this.sending || !this.sock?.isConnectionOpen()) return;
      this.sending = true;
      try {
        const blob: Blob | null = await new Promise((resolve) => this.robotRenderer!.domElement.toBlob(resolve, 'image/jpeg', 0.9));
        if (blob) this.sock.sendBinary(await blob.arrayBuffer());
      } catch {
        /* 무시 */
      } finally {
        this.sending = false;
      }
    }

    @onConnectedBodyShadow
    render() {
      return `
      <style>
        :host { display: block; padding: 16px; }
        .row { display: flex; gap: 12px; flex-wrap: wrap; }
        section { border: 1px solid var(--border); border-radius: var(--radius); padding: 14px; background: var(--surface); }
        section h2 { margin: 0 0 10px; font-size: 13px; font-weight: 700; color: var(--muted); text-transform: uppercase; letter-spacing: 0.06em; }
        canvas { border-radius: 8px; background: #000; }
        .meta { font-size: 13px; color: var(--muted); font-family: var(--font-mono); }
        .toolbar { display: flex; gap: 8px; align-items: center; margin-bottom: 12px; }
        button { font: inherit; font-size: 13px; font-weight: 600; color: var(--text); background: var(--surface); border: 1px solid var(--border); border-radius: 8px; padding: 8px 14px; cursor: pointer; }
        button:hover { background: var(--surface2); }
        button.on { background: var(--green-dim); border-color: var(--green); color: var(--green); }
      </style>
      <div class="toolbar">
        <button data-href="/">← 개요</button>
        <button class="stream-toggle">■ 송출</button>
        <button class="zero-motors">0으로 맞추기</button>
        <span class="meta stream-state">꺼짐 (카메라 없음)</span>
      </div>
      <p class="meta joints">관절 —</p>
      <p class="meta pillar-pos">기둥 x +0.500 y +0.000 m (월드 뷰에서 드래그로 이동)</p>
      <div class="row">
        <section><h2>로봇 카메라 (→ fake_camera)</h2><canvas class="robot" width="640" height="480" style="width:420px"></canvas></section>
        <section><h2>world (드래그: 이동 · Shift: 높이 · 오른쪽/Ctrl: 시점 · 휠: 줌)</h2><canvas class="world" width="420" height="300"></canvas></section>
      </div>
      <p class="meta">기구: 서버 URDF(/api/center/urdf) 파싱 렌더. 360° 배경·HSV 판정은 후속 이식 (scene.py 기준).</p>
      <p class="meta urdf-note"></p>`;
    }
  }

  return w.customElements.whenDefined(tagName);
};

export default defineWorldPage;
