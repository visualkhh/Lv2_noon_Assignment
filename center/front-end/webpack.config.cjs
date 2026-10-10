const path = require('path');
const HtmlWebpackPlugin = require('html-webpack-plugin');
const alias = require('../webpack.alias.cjs');

module.exports = {
  mode: 'development',
  devtool: 'source-map',
  entry: path.resolve(__dirname, './index.ts'),
  output: {
    path: path.resolve(__dirname, '../dist-front-end'),
    filename: 'bundle.js',
    clean: true
  },
  resolve: {
    extensions: ['.ts', '.js', '.html', '.css'],
    // submodule src에서 걸린 bare import(reflect-metadata·tslib 등)는 center/node_modules에서 찾음
    modules: [path.resolve(__dirname, '../node_modules'), 'node_modules'],
    alias: alias(path.resolve(__dirname, '..'))
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
        exclude: /node_modules/
      },
      { test: /\.html$/, use: 'raw-loader' },
      { test: /\.css$/, use: ['raw-loader'] }
    ]
  },
  plugins: [
    new HtmlWebpackPlugin({
      template: path.resolve(__dirname, './index.html'),
      scriptLoading: 'defer'
    })
  ],
  devServer: {
    port: 3003,
    hot: true,
    host: 'localhost',
    historyApiFallback: true
  }
};
