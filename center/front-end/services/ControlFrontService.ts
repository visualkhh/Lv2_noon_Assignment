import { sim } from '@dooboostore/simple-boot';
import { SymbolIntentApiServiceConfig } from '@dooboostore/simple-boot-http-server';
import factory from '@dooboostore/simple-boot-http-server/proxy/SymbolIntentApiServiceProxy';
import { ControlService } from '@app-src/services/ControlService';

export default (container: symbol) => {
  @sim({ symbol: ControlService.SYMBOL, container, proxy: factory({ container }) })
  class ControlFrontService implements ControlService {
    async listTargets(
      request: Record<string, never>,
      data: (config: SymbolIntentApiServiceConfig<Record<string, never>>) => Promise<ControlService.ListTargetsResponse>
    ): Promise<ControlService.ListTargetsResponse> {
      return data({ body: request });
    }
    async getStatus(
      request: { target: string },
      data: (config: SymbolIntentApiServiceConfig<{ target: string }>) => Promise<ControlService.StatusResponse>
    ): Promise<ControlService.StatusResponse> {
      return data({ body: request });
    }
    async getMotors(
      request: { target: string },
      data: (config: SymbolIntentApiServiceConfig<{ target: string }>) => Promise<ControlService.MotorsResponse>
    ): Promise<ControlService.MotorsResponse> {
      return data({ body: request });
    }
    async resetMotors(
      request: { target: string },
      data: (config: SymbolIntentApiServiceConfig<{ target: string }>) => Promise<{ ok: boolean }>
    ): Promise<{ ok: boolean }> {
      return data({ body: request });
    }
    async getImages(
      request: { target: string },
      data: (config: SymbolIntentApiServiceConfig<{ target: string }>) => Promise<ControlService.ImageItem[]>
    ): Promise<ControlService.ImageItem[]> {
      return data({ body: request });
    }
    async getSerial(
      request: { target: string; kind: 'in' | 'out' },
      data: (config: SymbolIntentApiServiceConfig<{ target: string; kind: 'in' | 'out' }>) => Promise<ControlService.SerialResponse>
    ): Promise<ControlService.SerialResponse> {
      return data({ body: request });
    }
    async getEcho(
      request: { target: string; name: string },
      data: (config: SymbolIntentApiServiceConfig<{ target: string; name: string }>) => Promise<ControlService.EchoResponse>
    ): Promise<ControlService.EchoResponse> {
      return data({ body: request });
    }
    async appendSerial(
      request: { target: string; line: string },
      data: (config: SymbolIntentApiServiceConfig<{ target: string; line: string }>) => Promise<{ ok: boolean; error?: string }>
    ): Promise<{ ok: boolean; error?: string }> {
      return data({ body: request });
    }
    async setStreaming(
      request: { target: string; on: boolean },
      data: (config: SymbolIntentApiServiceConfig<{ target: string; on: boolean }>) => Promise<{ ok: boolean; streaming: boolean }>
    ): Promise<{ ok: boolean; streaming: boolean }> {
      return data({ body: request });
    }
    async putFrame(
      request: { target: string; pngBase64: string },
      data: (config: SymbolIntentApiServiceConfig<{ target: string; pngBase64: string }>) => Promise<{ ok: boolean; error?: string }>
    ): Promise<{ ok: boolean; error?: string }> {
      return data({ body: request });
    }
  }
};
