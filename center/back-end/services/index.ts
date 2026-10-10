import { getSim } from '@dooboostore/simple-boot';

export * from './ControlBackService';

import { ControlBackService } from './ControlBackService';

export const pairServices = [getSim(ControlBackService)];
