/**
 * `RefreshControl` for QNX.
 *
 * QNX targets do not have a native pull-to-refresh gesture, so this is a
 * minimal no-op. It renders nothing and ignores `refreshing` and `onRefresh`.
 */

'use strict';

const React = require('react');

function RefreshControl() {
  return null;
}

RefreshControl.defaultProps = {
  refreshing: false,
};

module.exports = RefreshControl;
module.exports.default = RefreshControl;
