const path = require('path');
const alias = require('../webpack.alias.cjs');

module.exports = {
  target: 'node',
  mode: 'development',
  devtool: 'cheap-module-source-map',
  entry: path.resolve(__dirname, 'index.ts'),
  output: {
    path: path.resolve(__dirname, '../dist-back-end'),
    filename: 'index.cjs',
    sourceMapFilename: '[file].map',
    clean: true
  },
  module: {
    rules: [
      {
        test: /\.ts$/,
        use: {
          loader: 'ts-loader',
          options: {
            configFile: path.resolve(__dirname, '../tsconfig.json'),
            transpileOnly: true
          }
        },
        exclude: /node_modules\/(?!@dooboostore)/
      },
      // dom-parser 패키지는 sideEffects:false라 묶으면 registerNodeClasses가
      // 트리셰이킹되어 TreeWalker is not a constructor로 죽음 — submodule src는 살림
      {
        test: /packages\/@dooboostore\/dom-parser\/src/,
        sideEffects: true
      }
    ]
  },
  resolve: {
    extensions: ['.ts', '.js'],
    // submodule src에서 걸린 bare import(ws·jsdom·reflect-metadata 등)는 center/node_modules에서 찾음
    modules: [path.resolve(__dirname, '../node_modules'), 'node_modules'],
    alias: alias(path.resolve(__dirname, '..'))
  },
  externals: {
    playwright: 'commonjs playwright',
    'playwright-core': 'commonjs playwright-core'
  },
  optimization: { minimize: false },
  node: { __dirname: true }
};
