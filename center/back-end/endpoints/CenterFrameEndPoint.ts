import fs from 'fs';
import path from 'path';
import type { RawData } from 'ws';
import type { WebSocketEndPoint, WebSocketSet } from '@dooboostore/simple-boot-http-server/endpoints/WebSocketEndPoint';
import { findRoot } from '../debugRoots';

// 로봇 시점 프레임 수신 — World 페이지가 ws 바이너리로 쏘면 input-live/frame.png에 저장.
// (intent POST 대신: 연결 1개 유지, base64 없이 바이너리 그대로)
// ws://host/api/center/frames?target=target1
export class CenterFrameEndPoint implements WebSocketEndPoint {
  message(wsSet: WebSocketSet, data: { data: RawData; isBinary: boolean }) {
    if (!data.isBinary) return;
    let url: URL;
    try {
      url = new URL(wsSet.request.url ?? '', 'http://x');
    } catch {
      return;
    }
    if (url.pathname !== '/api/center/frames') return;
    const root = findRoot(url.searchParams.get('target') ?? '');
    if (!root) return;
    const buf = Buffer.isBuffer(data.data)
      ? data.data
      : Array.isArray(data.data)
        ? Buffer.concat(data.data)
        : Buffer.from(data.data as ArrayBuffer);
    if (buf.length === 0 || buf.length > 5 * 1024 * 1024) return;
    try {
      const live = path.join(root, 'input-live', 'frame.png');
      fs.mkdirSync(path.dirname(live), { recursive: true });
      const tmp = `${live}.tmp-${process.pid}`;
      fs.writeFileSync(tmp, buf);
      fs.renameSync(tmp, live); // 반쯤 쓰인 파일을 fake_camera가 읽지 않게 교체
    } catch {
      /* 디스크 에러 무시 */
    }
  }
}
