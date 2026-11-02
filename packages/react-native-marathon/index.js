/**
 * Marathon-only JS overrides. All other compatible React Native Qt view
 * overrides are shared by the QNX platform at the Metro resolution layer.
 */
'use strict';
const path = require('node:path');
const OVERRIDES_ROOT = path.join(__dirname, 'Libraries');
module.exports = { OVERRIDES_ROOT };
