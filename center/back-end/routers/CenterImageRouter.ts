import fs from 'fs';
import { Sim, Router, Route } from '@dooboostore/simple-boot';
import { GET, ReqSearchParamsObj, NotFoundError } from '@dooboostore/simple-boot-http-server';
import { environment } from '../environments/environment';
import { findRoot, resolveImageFile } from '../debugRoots';

// 이미지 스트리밍 라우터 — intent(폴링+base64) 대신:
//   GET /api/center/stream?target=&name=  MJPEG multipart (img 태그 src에 바로 — 계속 갱신)
// 핸들러가 async generator를 리턴하면 프레임워크가 yield마다 바로 body로 쏴줌.
// name은 getImages가 준 이름 그대로 (output-images/* 또는 /topic/...).
@Sim
@Router({ path: '/api/center' })
export class CenterImageRouter {
  @Route({ path: '/stream' })
  @GET({ res: { contentType: 'multipart/x-mixed-replace; boundary=frame', header: { 'Cache-Control': 'no-cache' } } })
  stream(search: ReqSearchParamsObj, signal: AbortSignal) {
    // generator 본문은 첫 next() 때까지 실행 안 돼서 여기서 검증해야 404가 헤더에 탐
    const file = this.resolve(String(search.target ?? ''), String(search.name ?? ''));
    if (!file) throw new NotFoundError({ message: 'no such image' });
    return this.frames(file, signal);
  }

  private async *frames(file: string, signal: AbortSignal): AsyncGenerator<Buffer> {
    const partType = file.endsWith('.png') ? 'image/png' : 'image/jpeg';
    let lastMtime = 0;
    try {
      while (!signal.aborted) {
        try {
          const st = fs.statSync(file);
          if (st.mtimeMs !== lastMtime && st.size > 0 && st.size <= 5 * 1024 * 1024) {
            lastMtime = st.mtimeMs;
            const buf = fs.readFileSync(file);
            yield Buffer.concat([
              Buffer.from(`--frame\r\nContent-Type: ${partType}\r\nContent-Length: ${buf.length}\r\n\r\n`),
              buf,
              Buffer.from('\r\n')
            ]);
          }
        } catch {
          /* 지워지면 다음 틱에 재시도 */
        }
        await new Promise<void>((resolve, reject) => {
          // abort 리스너는 tick이 정상 종료돼도 남으니 직접 뗌 — 안 떼면 100ms마다 쌓여 느린 누수
          const t = setTimeout(() => (signal.removeEventListener('abort', onAbort), resolve()), 100); // ~10fps
          const onAbort = () => (clearTimeout(t), reject(signal.reason));
          signal.addEventListener('abort', onAbort, { once: true });
        });
      }
    } finally {
      /* 클라이언트 disconnect → signal abort → generator 종료 */
    }
  }

  private resolve(target: string, name: string): string | null {
    const root = findRoot(target);
    if (!root) return null;
    return resolveImageFile(root, name);
  }

  // 기구 URDF 원문 — World 페이지가 받아 파싱해서 그림 (하드코딩 치수 대신)
  @Route({ path: '/urdf' })
  @GET({ res: { contentType: 'application/xml' } })
  urdf() {
    try {
      return fs.readFileSync(environment.urdfPath, 'utf-8');
    } catch {
      throw new NotFoundError({ message: 'no urdf configured' });
    }
  }
}
