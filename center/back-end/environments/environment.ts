import path from 'path';
import process from 'process';

// debug 폴더 = target 1개(1 ROS_DOMAIN_ID). 여러 개면 콜론(:)으로 구분.
// 기본값: 이 저장소의 lv2_module5/debug
const defaultDebug = path.resolve(process.cwd(), '..', 'lv2_module5', 'debug');

const argv = (name: string): string | undefined => {
  const prefix = `--${name}=`;
  const hit = process.argv.find((it) => it.startsWith(prefix));
  return hit ? hit.slice(prefix.length) : undefined;
};

const roots = (argv('debug-roots') ?? process.env.CENTER_DEBUG_ROOTS ?? defaultDebug)
  .split(':')
  .map((s) => s.trim())
  .filter(Boolean);

export const environment = {
  frontDistPath: path.resolve(process.cwd(), 'dist-front-end'),
  frontDistIndexFileName: 'index.html',
  httpServerConfig: {
    listen: { port: Number(argv('port') ?? process.env.PORT ?? 3032) }
  },
  debugRoots: roots,
  // 기구 URDF: World 페이지가 받아서 그림 (기본 = 저장소의 테스트베드 URDF)
  urdfPath: argv('urdf') ?? process.env.CENTER_URDF ?? path.resolve(process.cwd(), '..', 'docker', 'test-controller', 'pan_tilt.urdf')
};
