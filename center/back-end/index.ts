import 'reflect-metadata';
import { SimpleBootHttpSSRServer } from '@dooboostore/simple-boot-http-server-ssr/SimpleBootHttpSSRServer';
import { HttpSSRServerOption } from '@dooboostore/simple-boot-http-server-ssr/option/HttpSSRServerOption';
import { environment } from './environments/environment';
import { SimpleBootHttpServer } from '@dooboostore/simple-boot-http-server/SimpleBootHttpServer';
import bootfactory from '@app-src/bootfactory';
import { ResourceFilter } from '@dooboostore/simple-boot-http-server/filters/ResourceFilter';
import { RequestResponse } from '@dooboostore/simple-boot-http-server/models/RequestResponse';
import { pairServices } from '@app-back-end/services';
import { CenterImageRouter } from '@app-back-end/routers/CenterImageRouter';
import { CenterFrameEndPoint } from '@app-back-end/endpoints/CenterFrameEndPoint';
import { IntentSchemeFilter } from '@dooboostore/simple-boot-http-server/filters/IntentSchemeFilter';
import { SSRSimpleWebComponentDomParserFilter } from '@dooboostore/simple-boot-http-server-ssr';
import { Runnable, UrlUtils } from '@dooboostore/core';

type RunParams = Record<string, never>;

class Server implements Runnable<void, RunParams> {
  async run() {
    const otherInstanceSim = new Map<any, any>();
    let ssr!: SimpleBootHttpSSRServer;

    const ssrFilter = new SSRSimpleWebComponentDomParserFilter({
      frontDistPath: environment.frontDistPath,
      frontDistIndexFileName: environment.frontDistIndexFileName,
      intentServices: pairServices.filter(Boolean),
      registerComponents: async (window: any, rr: RequestResponse, sim: Map<symbol, any>) => {
        const { app } = await bootfactory(window, sim, UrlUtils.getUrlPath(window.location));
        return app;
      },
      ssrExcludeFilter: (rr: RequestResponse) => {
        // 정적 자원·이미지 스트림에 대해서는 SSR을 시도하지 않음
        if ((rr.reqUrlPathName ?? '').startsWith('/api/')) return true;
        return /\.(js|css|map|ico|png|jpg|jpeg|gif|json|xml|txt)$/.test(rr.reqUrlPathName);
      }
    });

    const resourceFilter = new ResourceFilter(environment.frontDistPath, [
      /\.ico/,
      /\.png$/,
      /\.map$/,
      /\.json$/,
      '/bundle.js'
    ]);

    const option = new HttpSSRServerOption(
      {
        listen: environment.httpServerConfig.listen,
        filters: [
          { isSupport: true, filter: resourceFilter },
          { isSupport: true, filter: ssrFilter },
          { isSupport: true, filter: IntentSchemeFilter }
        ],
        webSocketEndPoints: [new CenterFrameEndPoint()]
      },
      { rootRouter: CenterImageRouter }
    );

    option.listen.hostname = '0.0.0.0';
    option.listen.listeningListener = (server: SimpleBootHttpServer) => {
      console.log(`center startUP! listening on ${server.option.address}`);
      console.log(`debug roots: ${environment.debugRoots.join(', ')}`);
    };

    ssr = new SimpleBootHttpSSRServer(option);
    await ssr.run(otherInstanceSim);
    return ssr;
  }
}

new Server().run().then(() => {
  console.log('server started!!');
});
console.log('server wait...');
