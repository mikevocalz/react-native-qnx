/**
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 * QNX setup mirrors RN 0.88.1 setUpDefaultReactNativeEnvironment's runtime
 * initialization, without Apple/Android-only developer tooling. Merely calling
 * its enableDeveloperTools=false variant still makes Metro resolve those
 * platform-only static requires in a development bundle.
 */
'use strict';

require('react-native/Libraries/Core/setUpGlobals');
require('react-native/src/private/setup/setUpDOM').default();
require('react-native/src/private/setup/setUpPerformance').default();
require('react-native/Libraries/Core/polyfillPromise');
require('react-native/Libraries/Core/setUpTimers');
require('react-native/Libraries/Core/setUpErrorHandling');
require('react-native/Libraries/Core/setUpRegeneratorRuntime');
require('react-native/Libraries/Core/setUpXHR');
require('react-native/Libraries/Core/setUpAlert');
require('react-native/Libraries/Core/setUpNavigator');
require('react-native/Libraries/Core/setUpBatchedBridge');
require('react-native/Libraries/Core/setUpSegmentFetcher');
require('react-native/Libraries/ReactNative/AppRegistry');

const flags = require('react-native/src/private/featureflags/ReactNativeFeatureFlags');
if (flags.enableIntersectionObserverByDefault()) {
  require('react-native/src/private/setup/setUpIntersectionObserver').default();
}
if (flags.enableMutationObserverByDefault()) {
  require('react-native/src/private/setup/setUpMutationObserver').default();
}
if (flags.enableResizeObserverByDefault()) {
  require('react-native/src/private/setup/setUpResizeObserver').default();
}

// No React DevTools/LogBox/HMR installation is claimed by serving a dev bundle.
// A matching, explicitly enabled native development runtime owns those hooks.
