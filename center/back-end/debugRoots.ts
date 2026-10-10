import fs from 'fs';
import path from 'path';
import { environment } from './environments/environment';

// debug 폴더 = target 1개(1 ROS_DOMAIN_ID). 여러 경로면 target1, target2, ...
export const listRoots = () => environment.debugRoots.map((p, i) => ({ id: `target${i + 1}`, path: p }));

export const findRoot = (target: string): string | null => {
  const hit = listRoots().find((r) => r.id === target);
  if (!hit) return null;
  try {
    return fs.statSync(hit.path).isDirectory() ? hit.path : null;
  } catch {
    return null;
  }
};

const IMAGE_EXT = new Set(['.png', '.jpg', '.jpeg']);

// OverviewPage가 쓰는 이미지 이름 → debug 루트 안 실제 파일. 루트 밖으로는 절대 안 나감.
export const resolveImageFile = (root: string, name: string): string | null => {
  let rel: string;
  if (name.startsWith('output-images/')) {
    rel = name;
  } else if (name.startsWith('/')) {
    rel = path.join('topic', name.slice(1), 'image.jpg');
  } else {
    return null;
  }
  const full = path.resolve(root, rel);
  if (!full.startsWith(path.resolve(root) + path.sep)) return null;
  if (!IMAGE_EXT.has(path.extname(full).toLowerCase())) return null;
  try {
    return fs.statSync(full).isFile() ? full : null;
  } catch {
    return null;
  }
};

export const listImageNames = (root: string): { name: string; file: string }[] => {
  const found: { name: string; file: string }[] = [];
  const walk = (dir: string) => {
    let entries: fs.Dirent[] = [];
    try {
      entries = fs.readdirSync(dir, { withFileTypes: true });
    } catch {
      return;
    }
    for (const e of entries) {
      const full = path.join(dir, e.name);
      if (e.isDirectory()) walk(full);
      else if (IMAGE_EXT.has(path.extname(e.name).toLowerCase())) {
        if (dir === path.join(root, 'output-images')) found.push({ name: `output-images/${e.name}`, file: full });
        else if (path.basename(full) === 'image.jpg') {
          found.push({ name: '/' + path.relative(path.join(root, 'topic'), path.dirname(full)).split(path.sep).join('/'), file: full });
        }
      }
    }
  };
  walk(path.join(root, 'output-images'));
  walk(path.join(root, 'topic'));
  found.sort((a, b) => (a.name < b.name ? -1 : 1));
  return found;
};
