import bootfactory from '@app-src/bootfactory';
import { UrlUtils } from '@dooboostore/core';
import { serviceFactories } from './services';

console.log('🚀 Center App Starting...');

const w = window;

bootfactory(w, serviceFactories, UrlUtils.getUrlPath(w.location));
