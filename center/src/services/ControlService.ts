import { RequestResponse, SymbolIntentApiServiceConfig } from '@dooboostore/simple-boot-http-server';

// 웹 통제실 — debug 폴더(= 1 ROS_DOMAIN_ID)를 target 하나로 본다.
// DEBUG_ROOTS에 여러 경로를 두면 target이 여러 개 (향후 Pi 여러 대).
export namespace ControlService {
  export const SYMBOL = Symbol.for('ControlService');

  export type Target = { id: string; path: string };

  // topic/<이름>/message 최신값 + 갱신 경과
  export type StatusItem = { name: string; ageS: number | null; data: unknown; type: string | null };

  // 변화량 명령 누적 관절각 [deg] (run-controller.py MotorAccumulator와 같은 계산)
  export type MotorAngles = { pan: number; tilt: number; count: number; last: [number, number] | null };
  export type MotorsResponse = { joint: MotorAngles; serial: MotorAngles; source: string };

  // resetMotors: 지금부터 다시 누적 (tkinter 통제실 [0으로 맞추기]와 같음).
  // serial-out은 계속 쌓이므로, 백엔드가 현 시점 누적값을 베이스라인으로 잡고 그 뒤만 셈.

  export type ImageItem = { name: string; mtimeMs: number };
  // 이미지 바이트는 intent로 안 나감 — <img src="/api/center/stream?target=&name="> (MJPEG) 또는
  // /api/center/image (스틸). getImages는 이름+mtime 목록만 (가벼운 JSON 폴링용).

  export type ListTargetsResponse = { targets: Target[] };
  export type StatusResponse = { items: StatusItem[] };
  export type SerialResponse = { lines: string[] };
  export type EchoResponse = { exists: boolean; lines: string[] };
}

export interface ControlService {
  listTargets(
    request: Record<string, never>,
    data?: RequestResponse | ((config: SymbolIntentApiServiceConfig<Record<string, never>>) => Promise<ControlService.ListTargetsResponse>)
  ): Promise<ControlService.ListTargetsResponse>;
  getStatus(
    request: { target: string },
    data?: RequestResponse | ((config: SymbolIntentApiServiceConfig<{ target: string }>) => Promise<ControlService.StatusResponse>)
  ): Promise<ControlService.StatusResponse>;
  getMotors(
    request: { target: string },
    data?: RequestResponse | ((config: SymbolIntentApiServiceConfig<{ target: string }>) => Promise<ControlService.MotorsResponse>)
  ): Promise<ControlService.MotorsResponse>;
  resetMotors(
    request: { target: string },
    data?: RequestResponse | ((config: SymbolIntentApiServiceConfig<{ target: string }>) => Promise<{ ok: boolean }>)
  ): Promise<{ ok: boolean }>;
  getImages(
    request: { target: string },
    data?: RequestResponse | ((config: SymbolIntentApiServiceConfig<{ target: string }>) => Promise<ControlService.ImageItem[]>)
  ): Promise<ControlService.ImageItem[]>;
  getSerial(
    request: { target: string; kind: 'in' | 'out' },
    data?: RequestResponse | ((config: SymbolIntentApiServiceConfig<{ target: string; kind: 'in' | 'out' }>) => Promise<ControlService.SerialResponse>)
  ): Promise<ControlService.SerialResponse>;
  // 토픽별 echo tail (컨테이너 test-logger가 쓰는 debug/topic/<이름>/echo)
  getEcho(
    request: { target: string; name: string },
    data?: RequestResponse | ((config: SymbolIntentApiServiceConfig<{ target: string; name: string }>) => Promise<ControlService.EchoResponse>)
  ): Promise<ControlService.EchoResponse>;
  // serial-in 한 줄 쓰기 (OpenCR 응답 흉내 — run-controller.py send_serial_in과 같음)
  appendSerial(
    request: { target: string; line: string },
    data?: RequestResponse | ((config: SymbolIntentApiServiceConfig<{ target: string; line: string }>) => Promise<{ ok: boolean; error?: string }>)
  ): Promise<{ ok: boolean; error?: string }>;
  // 영상 송출 끄기 = frame.png 삭제 (fake_camera 발행 중단). 켜기는 World 페이지의 three.js 렌더가 putFrame으로 프레임 전송
  setStreaming(
    request: { target: string; on: boolean },
    data?: RequestResponse | ((config: SymbolIntentApiServiceConfig<{ target: string; on: boolean }>) => Promise<{ ok: boolean; streaming: boolean }>)
  ): Promise<{ ok: boolean; streaming: boolean }>;
  // three.js 렌더 프레임을 live 파일로 저장 → fake_camera(live) → perception → /target 닫힌 루프
  putFrame(
    request: { target: string; pngBase64: string },
    data?: RequestResponse | ((config: SymbolIntentApiServiceConfig<{ target: string; pngBase64: string }>) => Promise<{ ok: boolean; error?: string }>)
  ): Promise<{ ok: boolean; error?: string }>;
}
