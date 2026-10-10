// center 공용 webpack alias — tsconfig.json paths와 같은 규칙.
// @dooboostore/* 는 repo 루트 submodule(packages/)의 TS 원본으로 매핑한다.
// (npm 발행판은 소스와 어긋나므로 쓰지 않음)
const path = require('path');

module.exports = (appDir) => {
  const pkg = path.join(appDir, '..', 'packages', '@dooboostore');
  return {
    '@app-src': path.join(appDir, 'src'),
    '@app-back-end': path.join(appDir, 'back-end'),
    '@app-front-end': path.join(appDir, 'front-end'),
    '@dooboostore/algorithm': path.join(pkg, 'algorithm/src'),
    '@dooboostore/core': path.join(pkg, 'core/src'),
    '@dooboostore/core-node': path.join(pkg, 'core-node/src'),
    '@dooboostore/core-web': path.join(pkg, 'core-web/src'),
    '@dooboostore/dom-parser': path.join(pkg, 'dom-parser/src'),
    '@dooboostore/dom-render': path.join(pkg, 'dom-render/src'),
    '@dooboostore/lib-node': path.join(pkg, 'lib-node/src'),
    '@dooboostore/lib-web': path.join(pkg, 'lib-web/src'),
    '@dooboostore/simple-boot': path.join(pkg, 'simple-boot/src'),
    '@dooboostore/simple-boot-front': path.join(pkg, 'simple-boot-front/src'),
    '@dooboostore/simple-boot-http-server': path.join(pkg, 'simple-boot-http-server/src'),
    '@dooboostore/simple-boot-http-server-ssr': path.join(pkg, 'simple-boot-http-server-ssr/src'),
    '@dooboostore/simple-web-component': path.join(pkg, 'simple-web-component/src'),
    '@dooboostore/simple-web-component-library': path.join(pkg, 'simple-web-component-library/src')
  };
};
