/**
 * `RefreshControl` for QNX.
 *
 * Renders nothing. QNX has no native pull view, and upstream's ScrollView would
 * not mount this element anyway (it does so only for ios and android). The QNX
 * ScrollView reads this element's `onRefresh`, `refreshing` and `enabled` props
 * and calls `onRefresh` when a finger pulls down from the top
 * (Libraries/Components/ScrollView/PullToRefresh.js). This module holds no
 * state and draws no indicator.
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
