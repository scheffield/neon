/**
 * @param {import('preact-cli').Config} config - Original webpack config
 * @param {import('preact-cli').Env} env - Current environment info
 * @param {import('preact-cli').Helpers} helpers - Object with useful helpers for working with the webpack config
 */
export default (config, env, helpers) => {
  // devServer doesn't exist on production builds
  if (!env.isProd) {
    config.devServer.proxy = [
      {
        path: '/_api/**',
        target: 'http://192.168.2.183',
      }
    ];
  }
  if (env.isProd) {
    // from https://github.com/preactjs/preact-cli/wiki/Config-Recipes#disabling-source-maps-using-plugin-helpers
    config.devtool = false;
    // from https://ryaposov.com/posts/disabling-code-splitting-in-preact-cli-default-template
    // we don't need all that stuff for the very few files we use
    config.plugins.push(
      new helpers.webpack.optimize.LimitChunkCountPlugin({
        maxChunks: 1
      })
    );
    // from https://github.com/preactjs/preact-cli/wiki/Config-Recipes#removing-plugin-loader-or-rule
    // and https://github.com/preactjs/preact-cli/blob/master/packages/cli/src/lib/webpack/push-manifest.js
    let { index } = helpers.getPluginsByName(config, 'PushManifestPlugin')[0]
    config.plugins.splice(index, 1)

    // delete config.entry.polyfills;
    // console.log(config)
  }
}