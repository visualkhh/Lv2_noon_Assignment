import defineCenterRouter from './CenterRouter';
import defineOverviewPage from './OverviewPage';
import defineTopicsPage from './TopicsPage';
import defineWorldPage from './WorldPage';

export const pageFactories = [defineCenterRouter, defineOverviewPage, defineTopicsPage, defineWorldPage];
