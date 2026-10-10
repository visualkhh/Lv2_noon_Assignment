import fs from 'fs';
import path from 'path';
import { sim as Sim } from '@dooboostore/simple-boot';
import { RequestResponse } from '@dooboostore/simple-boot-http-server';
import { ControlService } from '@app-src/services/ControlService';
import { listRoots, findRoot, listImageNames } from '@app-back-end/debugRoots';

const TARGET_LIMIT: [number, number] = [-180.0, 179.9]; // 펌웨어 config.h MIN/MAX_TARGET_DEG
const MAX_SERIAL_LINES = 200;

const emptyAngles = (): ControlService.MotorAngles => ({ pan: 0, tilt: 0, count: 0, last: null });

// resetMotors 베이스라인 (target별, 메모리 — 서버 재시작하면 사라짐)
const baselines = new Map<string, ControlService.MotorAngles>();

const addAngles = (acc: ControlService.MotorAngles, dPan: number, dTilt: number) => {
  const [lo, hi] = TARGET_LIMIT;
  acc.pan = Math.min(Math.max(acc.pan + dPan, lo), hi);
  acc.tilt = Math.min(Math.max(acc.tilt + dTilt, lo), hi);
  acc.last = [dPan, dTilt];
  acc.count += 1;
};

const readTailLines = (file: string, max: number): string[] => {
  try {
    const text = fs.readFileSync(file, 'utf-8');
    const lines = text.split('\n');
    if (lines.length && lines[lines.length - 1] === '') lines.pop();
    return lines.slice(-max);
  } catch {
    return [];
  }
};

const listFilesRecursive = (dir: string, out: string[] = []): string[] => {
  let entries: fs.Dirent[] = [];
  try {
    entries = fs.readdirSync(dir, { withFileTypes: true });
  } catch {
    return out;
  }
  for (const e of entries) {
    const full = path.join(dir, e.name);
    if (e.isDirectory()) listFilesRecursive(full, out);
    else out.push(full);
  }
  return out;
};

@Sim({ symbol: ControlService.SYMBOL })
export class ControlBackService implements ControlService {
  async listTargets(): Promise<ControlService.ListTargetsResponse> {
    return { targets: listRoots() };
  }

  async getStatus(request: { target: string }): Promise<ControlService.StatusResponse> {
    const root = findRoot(request?.target);
    if (!root) return { items: [] };
    const now = Date.now();
    const files = listFilesRecursive(path.join(root, 'topic')).filter((f) => path.basename(f) === 'message');
    const items: ControlService.StatusItem[] = files.map((f) => {
      const name = '/' + path.relative(path.join(root, 'topic'), path.dirname(f)).split(path.sep).join('/');
      let ageS: number | null = null;
      let data: unknown = null;
      let type: string | null = null;
      try {
        ageS = (now - fs.statSync(f).mtimeMs) / 1000;
        const msg = JSON.parse(fs.readFileSync(f, 'utf-8'));
        data = msg?.data ?? null;
        type = typeof msg?.type === 'string' ? msg.type : null;
      } catch {
        /* 파일이 지워지거나 깨지면 null 유지 */
      }
      return { name, ageS, data, type };
    });
    items.sort((a, b) => (a.name < b.name ? -1 : 1));
    return { items };
  }

  async getMotors(request: { target: string }): Promise<ControlService.MotorsResponse> {
    const root = findRoot(request?.target);
    const joint = emptyAngles();
    const serial = emptyAngles();
    if (!root) return { joint, serial, source: 'no-target' };
    // serial-out "M,Δpan,Δtilt"[deg] 전부 누적 (펌웨어와 같은 계산)
    for (const line of readTailLines(path.join(root, 'serial-out'), 100000)) {
      const parts = line.trim().split(',');
      if (parts.length === 3 && parts[0] === 'M') {
        const dp = Number(parts[1]);
        const dt = Number(parts[2]);
        if (Number.isFinite(dp) && Number.isFinite(dt)) addAngles(serial, dp, dt);
      }
    }
    // /motor_cmd 최신값 1건 (monitor_manager가 덮어쓴 message) — 누적 비교용 참고값
    // ponytail: 파일에는 최신 1건만 남으므로 과거 누적 복원은 불가. 정확한 누적은 프론트가 폴링하며 이어 붙임
    try {
      const msg = JSON.parse(fs.readFileSync(path.join(root, 'topic', 'motor_cmd', 'message'), 'utf-8'));
      const d = msg?.data ?? {};
      const names: string[] = d.name ?? [];
      const pos: number[] = d.position ?? [];
      const v = Object.fromEntries(names.map((n: string, i: number) => [n, pos[i]]));
      if (typeof v.pan_joint === 'number' && typeof v.tilt_joint === 'number') {
        joint.count = 1;
        joint.last = [(v.pan_joint * 180) / Math.PI, (v.tilt_joint * 180) / Math.PI];
      }
    } catch {
      /* 없음 */
    }
    const source = serial.count > 0 ? 'serial' : joint.count > 0 ? 'joint' : 'none';
    // 베이스라인 이후분만 (resetMotors로 잡은 시점부터 다시 누적)
    const base = request?.target ? baselines.get(request.target) : undefined;
    if (base) {
      serial.pan -= base.pan;
      serial.tilt -= base.tilt;
      serial.count = Math.max(0, serial.count - base.count);
    }
    return { joint, serial, source };
  }

  async resetMotors(request: { target: string }): Promise<{ ok: boolean }> {
    const root = findRoot(request?.target);
    if (!root || !request?.target) return { ok: false };
    // 현 시점 누적값을 통째로 베이스라인으로 저장 (getMotors와 같은 파싱)
    const acc = emptyAngles();
    for (const line of readTailLines(path.join(root, 'serial-out'), 100000)) {
      const parts = line.trim().split(',');
      if (parts.length === 3 && parts[0] === 'M') {
        const dp = Number(parts[1]);
        const dt = Number(parts[2]);
        if (Number.isFinite(dp) && Number.isFinite(dt)) addAngles(acc, dp, dt);
      }
    }
    baselines.set(request.target, acc);
    return { ok: true };
  }

  async getImages(request: { target: string }): Promise<ControlService.ImageItem[]> {
    // 이름+mtime만 — 바이트는 /api/center/stream(MJPEG)·/api/center/image(스틸)로
    const root = findRoot(request?.target);
    if (!root) return [];
    return listImageNames(root).map(({ name, file }) => {
      let mtimeMs = 0;
      try {
        mtimeMs = fs.statSync(file).mtimeMs;
      } catch {
        /* 지워지면 0 */
      }
      return { name, mtimeMs };
    });
  }

  async getSerial(request: { target: string; kind: 'in' | 'out' }): Promise<ControlService.SerialResponse> {
    const root = findRoot(request?.target);
    if (!root) return { lines: [] };
    const file = path.join(root, request?.kind === 'in' ? 'serial-in' : 'serial-out');
    return { lines: readTailLines(file, MAX_SERIAL_LINES) };
  }

  async getEcho(request: { target: string; name: string }): Promise<ControlService.EchoResponse> {
    const root = findRoot(request?.target);
    if (!root) return { exists: false, lines: [] };
    // topic/<이름>/echo 만 — 루트 밖으로 안 나감
    const name = String(request?.name ?? '');
    if (!name.startsWith('/')) return { exists: false, lines: [] };
    const full = path.resolve(root, 'topic', name.slice(1), 'echo');
    if (!full.startsWith(path.resolve(root, 'topic') + path.sep)) return { exists: false, lines: [] };
    try {
      if (!fs.statSync(full).isFile()) return { exists: false, lines: [] };
    } catch {
      return { exists: false, lines: [] };
    }
    return { exists: true, lines: readTailLines(full, MAX_SERIAL_LINES) };
  }

  async appendSerial(request: { target: string; line: string }): Promise<{ ok: boolean; error?: string }> {
    const root = findRoot(request?.target);
    if (!root) return { ok: false, error: 'unknown-target' };
    const line = String(request?.line ?? '').trim();
    if (!line) return { ok: false, error: 'empty-line' };
    try {
      fs.appendFileSync(path.join(root, 'serial-in'), line + '\n');
      return { ok: true };
    } catch (e) {
      return { ok: false, error: String(e) };
    }
  }

  async setStreaming(request: { target: string; on: boolean }): Promise<{ ok: boolean; streaming: boolean }> {
    const root = findRoot(request?.target);
    if (!root) return { ok: false, streaming: false };
    const live = path.join(root, 'input-live', 'frame.png');
    try {
      if (request?.on) {
        // 켜기는 World 페이지의 three.js 렌더가 putFrame으로 프레임을 보내야 실제 송출
        return { ok: true, streaming: fs.existsSync(live) };
      }
      fs.rmSync(live, { force: true });
      return { ok: true, streaming: false };
    } catch (e) {
      return { ok: false, streaming: false };
    }
  }

  async putFrame(request: { target: string; pngBase64: string }): Promise<{ ok: boolean; error?: string }> {
    const root = findRoot(request?.target);
    if (!root) return { ok: false, error: 'unknown-target' };
    const raw = String(request?.pngBase64 ?? '').replace(/^data:image\/\w+;base64,/, '');
    if (!raw) return { ok: false, error: 'empty-frame' };
    try {
      const buf = Buffer.from(raw, 'base64');
      if (buf.length === 0 || buf.length > 5 * 1024 * 1024) return { ok: false, error: 'bad-size' };
      const live = path.join(root, 'input-live', 'frame.png');
      fs.mkdirSync(path.dirname(live), { recursive: true });
      const tmp = `${live}.tmp-${process.pid}`;
      fs.writeFileSync(tmp, buf);
      fs.renameSync(tmp, live); // 반쯤 쓰인 파일을 fake_camera가 읽지 않게 교체
      return { ok: true };
    } catch (e) {
      return { ok: false, error: String(e) };
    }
  }
}
