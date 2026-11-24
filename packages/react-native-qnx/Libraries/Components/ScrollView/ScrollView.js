/**
 * QNX copy of React Native 0.88.1's Libraries/Components/ScrollView/ScrollView.js.
 *
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * Licensed under the MIT license found in the LICENSE file in the root directory
 * of the react-native source tree.
 *
 * Upstream mounts a `refreshControl` only when `Platform.OS` is `ios` or
 * `android`, so on `qnx` the element was dropped and its `onRefresh` could never
 * fire. The host has no native pull view to mount, so this copy detects the pull
 * from touch events instead (PullToRefresh.js).
 *
 * How this file was made: upstream's file with Flow types and comments removed
 * (Babel, @babel/plugin-transform-flow-strip-types over the Hermes parser), so
 * Prettier can check it, then formatted. Every functional difference is marked
 * `QNX`: the imports (a copy outside React Native's tree cannot use its relative
 * paths, so they are spelled `react-native/Libraries/...`, and the H/V native
 * components are the two qnx uses), the invariant, nullthrows and memoize-one stand-ins, the PullToRefresh import, the `_pullToRefresh` and `_scrollY` fields,
 * `_pullRefreshProps`, and the lines added to `_handleScroll` and the four touch
 * handlers. tests/js/override-paths-match-upstream.test.mjs pins the React Native
 * version this copy belongs to.
 */

// QNX: on qnx, upstream's H/V scroll components are these two. Importing them
// here avoids src/private, which React Native does not export.
import ScrollContentViewNativeComponent from 'react-native/Libraries/Components/ScrollView/ScrollContentViewNativeComponent';
import ScrollViewNativeComponent from 'react-native/Libraries/Components/ScrollView/ScrollViewNativeComponent';
const HScrollViewNativeComponent = ScrollViewNativeComponent;
const HScrollContentViewNativeComponent = ScrollContentViewNativeComponent;
const VScrollViewNativeComponent = ScrollViewNativeComponent;
const VScrollContentViewNativeComponent = ScrollContentViewNativeComponent;
import AnimatedImplementation from 'react-native/Libraries/Animated/AnimatedImplementation';
import FrameRateLogger from 'react-native/Libraries/Interaction/FrameRateLogger';
import { findNodeHandle } from 'react-native/Libraries/ReactNative/RendererProxy';
import UIManager from 'react-native/Libraries/ReactNative/UIManager';
import flattenStyle from 'react-native/Libraries/StyleSheet/flattenStyle';
import splitLayoutProps from 'react-native/Libraries/StyleSheet/splitLayoutProps';
import StyleSheet from 'react-native/Libraries/StyleSheet/StyleSheet';
import Dimensions from 'react-native/Libraries/Utilities/Dimensions';
import dismissKeyboard from 'react-native/Libraries/Utilities/dismissKeyboard';
import Platform from 'react-native/Libraries/Utilities/Platform';
import Keyboard from 'react-native/Libraries/Components/Keyboard/Keyboard';
import TextInputState from 'react-native/Libraries/Components/TextInput/TextInputState';
import View from 'react-native/Libraries/Components/View/View';
import processDecelerationRate from 'react-native/Libraries/Components/ScrollView/processDecelerationRate';
import Commands from 'react-native/Libraries/Components/ScrollView/ScrollViewCommands';
import ScrollViewContext, {
  HORIZONTAL,
  VERTICAL,
} from 'react-native/Libraries/Components/ScrollView/ScrollViewContext';
// QNX: pull-to-refresh gesture state.
import { createPullToRefresh } from './PullToRefresh';
import ScrollViewStickyHeader from 'react-native/Libraries/Components/ScrollView/ScrollViewStickyHeader';
import * as React from 'react';
import { cloneElement } from 'react';

// QNX: upstream imports these three from React Native's own dependencies, which a
// file outside React Native's tree cannot resolve under pnpm. They are the same
// small functions.
function invariant(condition, message) {
  if (!condition) {
    throw new Error(message);
  }
}
function nullthrows(value, message) {
  if (value != null) {
    return value;
  }
  throw new Error(message ?? 'Got unexpected ' + value);
}
function memoize(fn) {
  let called = false;
  let lastArg;
  let lastResult;
  return (arg) => {
    if (called && lastArg === arg) {
      return lastResult;
    }
    lastResult = fn(arg);
    lastArg = arg;
    called = true;
    return lastResult;
  };
}
const IS_ANIMATING_TOUCH_START_THRESHOLD_MS = 16;
class ScrollView extends React.Component {
  static Context = ScrollViewContext;
  constructor(props) {
    super(props);
    this._scrollAnimatedValue = new AnimatedImplementation.Value(this.props.contentOffset?.y ?? 0);
    this._scrollAnimatedValue.setOffset(this.props.contentInset?.top ?? 0);
  }
  _scrollAnimatedValueAttachment = null;
  _stickyHeaderRefs = new Map();
  _headerLayoutYs = new Map();
  _keyboardMetrics = null;
  _additionalScrollOffset = 0;
  _isTouching = false;
  // QNX: see PullToRefresh.js.
  _pullToRefresh = createPullToRefresh();
  _scrollY = 0;
  _lastMomentumScrollBeginTime = 0;
  _lastMomentumScrollEndTime = 0;
  _observedScrollSinceBecomingResponder = false;
  _becameResponderWhileAnimating = false;
  _preventNegativeScrollOffset = null;
  _animated = null;
  _subscriptionKeyboardWillShow = null;
  _subscriptionKeyboardWillHide = null;
  _subscriptionKeyboardDidShow = null;
  _subscriptionKeyboardDidHide = null;
  state = {
    layoutHeight: null,
  };
  componentDidMount() {
    this._keyboardMetrics = Keyboard.metrics();
    this._additionalScrollOffset = 0;
    this._subscriptionKeyboardWillShow = Keyboard.addListener(
      'keyboardWillShow',
      this.scrollResponderKeyboardWillShow
    );
    this._subscriptionKeyboardWillHide = Keyboard.addListener(
      'keyboardWillHide',
      this.scrollResponderKeyboardWillHide
    );
    this._subscriptionKeyboardDidShow = Keyboard.addListener(
      'keyboardDidShow',
      this.scrollResponderKeyboardDidShow
    );
    this._subscriptionKeyboardDidHide = Keyboard.addListener(
      'keyboardDidHide',
      this.scrollResponderKeyboardDidHide
    );
    this._updateAnimatedNodeAttachment();
  }
  componentDidUpdate(prevProps) {
    const prevContentInsetTop = prevProps.contentInset ? prevProps.contentInset.top : 0;
    const newContentInsetTop = this.props.contentInset ? this.props.contentInset.top : 0;
    if (prevContentInsetTop !== newContentInsetTop) {
      this._scrollAnimatedValue.setOffset(newContentInsetTop || 0);
    }
    this._updateAnimatedNodeAttachment();
  }
  componentWillUnmount() {
    if (this._subscriptionKeyboardWillShow != null) {
      this._subscriptionKeyboardWillShow.remove();
    }
    if (this._subscriptionKeyboardWillHide != null) {
      this._subscriptionKeyboardWillHide.remove();
    }
    if (this._subscriptionKeyboardDidShow != null) {
      this._subscriptionKeyboardDidShow.remove();
    }
    if (this._subscriptionKeyboardDidHide != null) {
      this._subscriptionKeyboardDidHide.remove();
    }
    if (this._scrollAnimatedValueAttachment) {
      this._scrollAnimatedValueAttachment.detach();
    }
  }
  getScrollResponder = () => {
    return this;
  };
  getScrollableNode = () => {
    return findNodeHandle(this.getNativeScrollRef());
  };
  getInnerViewNode = () => {
    return findNodeHandle(this._innerView.nativeInstance);
  };
  getInnerViewRef = () => {
    return this._innerView.nativeInstance;
  };
  getNativeScrollRef = () => {
    return this._scrollView.nativeInstance;
  };
  scrollTo = (options, deprecatedX, deprecatedAnimated) => {
    let x, y, animated;
    if (typeof options === 'number') {
      console.warn(
        '`scrollTo(y, x, animated)` is deprecated. Use `scrollTo({x: 5, y: 5, ' +
          'animated: true})` instead.'
      );
      y = options;
      x = deprecatedX;
      animated = deprecatedAnimated;
    } else if (options) {
      y = options.y;
      x = options.x;
      animated = options.animated;
    }
    const component = this.getNativeScrollRef();
    if (component == null) {
      return;
    }
    Commands.scrollTo(component, x || 0, y || 0, animated !== false);
  };
  scrollToEnd = (options) => {
    const animated = (options && options.animated) !== false;
    const component = this.getNativeScrollRef();
    if (component == null) {
      return;
    }
    Commands.scrollToEnd(component, animated);
  };
  flashScrollIndicators = () => {
    const component = this.getNativeScrollRef();
    if (component == null) {
      return;
    }
    Commands.flashScrollIndicators(component);
  };
  scrollResponderScrollNativeHandleToKeyboard = (
    nodeHandle,
    additionalOffset,
    preventNegativeScrollOffset
  ) => {
    this._additionalScrollOffset = additionalOffset || 0;
    this._preventNegativeScrollOffset = !!preventNegativeScrollOffset;
    if (this._innerView.nativeInstance == null) {
      return;
    }
    if (typeof nodeHandle === 'number') {
      UIManager.measureLayout(
        nodeHandle,
        nullthrows(findNodeHandle(this)),
        this._textInputFocusError,
        this._inputMeasureAndScrollToKeyboard
      );
    } else {
      nodeHandle.measureLayout(
        this._innerView.nativeInstance,
        this._inputMeasureAndScrollToKeyboard,
        this._textInputFocusError
      );
    }
  };
  scrollResponderZoomTo = (rect, animated) => {
    invariant(Platform.OS === 'ios', 'zoomToRect is not implemented');
    if ('animated' in rect) {
      this._animated = rect.animated;
      delete rect.animated;
    } else if (typeof animated !== 'undefined') {
      console.warn(
        '`scrollResponderZoomTo` `animated` argument is deprecated. Use `options.animated` instead'
      );
    }
    const component = this.getNativeScrollRef();
    if (component == null) {
      return;
    }
    Commands.zoomToRect(component, rect, animated !== false);
  };
  _textInputFocusError() {
    console.warn('Error measuring text field.');
  }
  _inputMeasureAndScrollToKeyboard = (left, top, width, height) => {
    let keyboardScreenY = Dimensions.get('window').height;
    const scrollTextInputIntoVisibleRect = () => {
      if (this._keyboardMetrics != null) {
        keyboardScreenY = this._keyboardMetrics.screenY;
      }
      let scrollOffsetY = top - keyboardScreenY + height + this._additionalScrollOffset;
      if (this._preventNegativeScrollOffset === true) {
        scrollOffsetY = Math.max(0, scrollOffsetY);
      }
      this.scrollTo({
        x: 0,
        y: scrollOffsetY,
        animated: true,
      });
      this._additionalScrollOffset = 0;
      this._preventNegativeScrollOffset = false;
    };
    if (this._keyboardMetrics == null) {
      setTimeout(() => {
        scrollTextInputIntoVisibleRect();
      }, 0);
    } else {
      scrollTextInputIntoVisibleRect();
    }
  };
  _getKeyForIndex(index, childArray) {
    const child = childArray[index];
    return child && child.key;
  }
  _updateAnimatedNodeAttachment() {
    if (this._scrollAnimatedValueAttachment) {
      this._scrollAnimatedValueAttachment.detach();
    }
    if (this.props.stickyHeaderIndices && this.props.stickyHeaderIndices.length > 0) {
      this._scrollAnimatedValueAttachment = AnimatedImplementation.attachNativeEvent(
        this.getNativeScrollRef(),
        'onScroll',
        [
          {
            nativeEvent: {
              contentOffset: {
                y: this._scrollAnimatedValue,
              },
            },
          },
        ]
      );
    }
  }
  _setStickyHeaderRef(key, ref) {
    if (ref) {
      this._stickyHeaderRefs.set(key, ref);
    } else {
      this._stickyHeaderRefs.delete(key);
    }
  }
  _onStickyHeaderLayout(index, event, key) {
    const { stickyHeaderIndices } = this.props;
    if (!stickyHeaderIndices) {
      return;
    }
    const childArray = React.Children.toArray(this.props.children);
    if (key !== this._getKeyForIndex(index, childArray)) {
      return;
    }
    const layoutY = event.nativeEvent.layout.y;
    this._headerLayoutYs.set(key, layoutY);
    const indexOfIndex = stickyHeaderIndices.indexOf(index);
    const previousHeaderIndex = stickyHeaderIndices[indexOfIndex - 1];
    if (previousHeaderIndex != null) {
      const previousHeader = this._stickyHeaderRefs.get(
        this._getKeyForIndex(previousHeaderIndex, childArray)
      );
      previousHeader && previousHeader.setNextHeaderY && previousHeader.setNextHeaderY(layoutY);
    }
  }
  // QNX: the refresh control's props when a pull may start now, else null. The
  // control element is not mounted, so its props are the whole contract.
  _pullRefreshProps() {
    const control = this.props.refreshControl;
    if (this.props.horizontal === true || control == null || control.props == null) {
      return null;
    }
    const { onRefresh, refreshing, enabled } = control.props;
    return typeof onRefresh === 'function' && refreshing !== true && enabled !== false
      ? control.props
      : null;
  }
  _handleScroll = (e) => {
    // QNX
    this._scrollY = e.nativeEvent.contentOffset.y;
    this._pullToRefresh.scrolled(this._scrollY);
    this._observedScrollSinceBecomingResponder = true;
    this.props.onScroll && this.props.onScroll(e);
  };
  _handleLayout = (e) => {
    if (this.props.invertStickyHeaders === true) {
      this.setState({
        layoutHeight: e.nativeEvent.layout.height,
      });
    }
    if (this.props.onLayout) {
      this.props.onLayout(e);
    }
  };
  _handleContentOnLayout = (e) => {
    const { width, height } = e.nativeEvent.layout;
    this.props.onContentSizeChange && this.props.onContentSizeChange(width, height);
  };
  _innerView = createRefForwarder((instance) => instance);
  _scrollView = createRefForwarder((nativeInstance) => {
    const publicInstance = Object.assign(nativeInstance, {
      getScrollResponder: this.getScrollResponder,
      getScrollableNode: this.getScrollableNode,
      getInnerViewNode: this.getInnerViewNode,
      getInnerViewRef: this.getInnerViewRef,
      getNativeScrollRef: this.getNativeScrollRef,
      scrollTo: this.scrollTo,
      scrollToEnd: this.scrollToEnd,
      flashScrollIndicators: this.flashScrollIndicators,
      scrollResponderZoomTo: this.scrollResponderZoomTo,
      scrollResponderScrollNativeHandleToKeyboard: this.scrollResponderScrollNativeHandleToKeyboard,
    });
    return publicInstance;
  });
  scrollResponderKeyboardWillShow = (e) => {
    this._keyboardMetrics = e.endCoordinates;
    this.props.onKeyboardWillShow && this.props.onKeyboardWillShow(e);
  };
  scrollResponderKeyboardWillHide = (e) => {
    this._keyboardMetrics = null;
    this.props.onKeyboardWillHide && this.props.onKeyboardWillHide(e);
  };
  scrollResponderKeyboardDidShow = (e) => {
    this._keyboardMetrics = e.endCoordinates;
    this.props.onKeyboardDidShow && this.props.onKeyboardDidShow(e);
  };
  scrollResponderKeyboardDidHide = (e) => {
    this._keyboardMetrics = null;
    this.props.onKeyboardDidHide && this.props.onKeyboardDidHide(e);
  };
  _handleMomentumScrollBegin = (e) => {
    this._lastMomentumScrollBeginTime = global.performance.now();
    this.props.onMomentumScrollBegin && this.props.onMomentumScrollBegin(e);
  };
  _handleMomentumScrollEnd = (e) => {
    FrameRateLogger.endScroll();
    this._lastMomentumScrollEndTime = global.performance.now();
    this.props.onMomentumScrollEnd && this.props.onMomentumScrollEnd(e);
  };
  _handleScrollBeginDrag = (e) => {
    FrameRateLogger.beginScroll();
    if (Platform.OS === 'android' && this.props.keyboardDismissMode === 'on-drag') {
      dismissKeyboard();
    }
    this.props.onScrollBeginDrag && this.props.onScrollBeginDrag(e);
  };
  _handleScrollEndDrag = (e) => {
    const { velocity } = e.nativeEvent;
    if (!this._isAnimating() && (!velocity || (velocity.x === 0 && velocity.y === 0))) {
      FrameRateLogger.endScroll();
    }
    this.props.onScrollEndDrag && this.props.onScrollEndDrag(e);
  };
  _isAnimating = () => {
    const now = global.performance.now();
    const timeSinceLastMomentumScrollEnd = now - this._lastMomentumScrollEndTime;
    const isAnimating =
      timeSinceLastMomentumScrollEnd < IS_ANIMATING_TOUCH_START_THRESHOLD_MS ||
      this._lastMomentumScrollEndTime < this._lastMomentumScrollBeginTime;
    return isAnimating;
  };
  _handleResponderGrant = (e) => {
    this._observedScrollSinceBecomingResponder = false;
    this.props.onResponderGrant && this.props.onResponderGrant(e);
    this._becameResponderWhileAnimating = this._isAnimating();
  };
  _handleResponderReject = () => {};
  _handleResponderRelease = (e) => {
    this._isTouching = e.nativeEvent.touches.length !== 0;
    this.props.onResponderRelease && this.props.onResponderRelease(e);
    if (typeof e.target === 'number') {
      if (__DEV__) {
        console.error(
          'Did not expect event target to be a number. Should have been a native component'
        );
      }
      return;
    }
    const currentlyFocusedTextInput = TextInputState.currentlyFocusedInput();
    if (
      currentlyFocusedTextInput != null &&
      this.props.keyboardShouldPersistTaps !== 'always' &&
      this._keyboardIsDismissible() &&
      e.target !== currentlyFocusedTextInput &&
      !this._observedScrollSinceBecomingResponder &&
      !this._becameResponderWhileAnimating
    ) {
      TextInputState.blurTextInput(currentlyFocusedTextInput);
    }
  };
  _handleResponderTerminationRequest = () => {
    return !this._observedScrollSinceBecomingResponder;
  };
  _handleScrollShouldSetResponder = () => {
    if (this.props.disableScrollViewPanResponder === true) {
      return false;
    }
    return this._isTouching;
  };
  _handleStartShouldSetResponder = (e) => {
    if (this.props.disableScrollViewPanResponder === true) {
      return false;
    }
    const currentlyFocusedInput = TextInputState.currentlyFocusedInput();
    if (
      this.props.keyboardShouldPersistTaps === 'handled' &&
      this._keyboardIsDismissible() &&
      e.target !== currentlyFocusedInput
    ) {
      return true;
    }
    return false;
  };
  _handleStartShouldSetResponderCapture = (e) => {
    if (this._isAnimating()) {
      return true;
    }
    if (this.props.disableScrollViewPanResponder === true) {
      return false;
    }
    const { keyboardShouldPersistTaps } = this.props;
    const keyboardNeverPersistTaps =
      !keyboardShouldPersistTaps || keyboardShouldPersistTaps === 'never';
    if (typeof e.target === 'number') {
      if (__DEV__) {
        console.error(
          'Did not expect event target to be a number. Should have been a native component'
        );
      }
      return false;
    }
    if (this._softKeyboardIsDetached()) {
      return false;
    }
    if (
      keyboardNeverPersistTaps &&
      this._keyboardIsDismissible() &&
      e.target != null &&
      !TextInputState.isTextInput(e.target)
    ) {
      return true;
    }
    return false;
  };
  _keyboardIsDismissible = () => {
    const currentlyFocusedInput = TextInputState.currentlyFocusedInput();
    const hasFocusedTextInput =
      currentlyFocusedInput != null && TextInputState.isTextInput(currentlyFocusedInput);
    const softKeyboardMayBeOpen =
      this._keyboardMetrics != null || this._keyboardEventsAreUnreliable();
    return hasFocusedTextInput && softKeyboardMayBeOpen;
  };
  _softKeyboardIsDetached = () => {
    return this._keyboardMetrics != null && this._keyboardMetrics.height === 0;
  };
  _keyboardEventsAreUnreliable = () => {
    return Platform.OS === 'android' && Platform.Version < 30;
  };
  _handleTouchEnd = (e) => {
    const nativeEvent = e.nativeEvent;
    this._isTouching = nativeEvent.touches.length !== 0;
    // QNX
    if (!this._isTouching) {
      this._pullToRefresh.end();
    }
    const { keyboardShouldPersistTaps } = this.props;
    const keyboardNeverPersistsTaps =
      !keyboardShouldPersistTaps || keyboardShouldPersistTaps === 'never';
    const currentlyFocusedTextInput = TextInputState.currentlyFocusedInput();
    if (
      currentlyFocusedTextInput != null &&
      e.target !== currentlyFocusedTextInput &&
      this._softKeyboardIsDetached() &&
      this._keyboardIsDismissible() &&
      keyboardNeverPersistsTaps
    ) {
      TextInputState.blurTextInput(currentlyFocusedTextInput);
    }
    this.props.onTouchEnd && this.props.onTouchEnd(e);
  };
  _handleTouchCancel = (e) => {
    this._isTouching = false;
    this._pullToRefresh.end(); // QNX
    this.props.onTouchCancel && this.props.onTouchCancel(e);
  };
  _handleTouchStart = (e) => {
    this._isTouching = true;
    // QNX
    this._pullToRefresh.start(
      e.nativeEvent.pageY,
      this._scrollY,
      e.nativeEvent.touches.length,
      this._pullRefreshProps() != null
    );
    this.props.onTouchStart && this.props.onTouchStart(e);
  };
  _handleTouchMove = (e) => {
    // QNX
    if (this._pullToRefresh.move(e.nativeEvent.pageY, e.nativeEvent.touches.length)) {
      const pull = this._pullRefreshProps();
      if (pull != null) {
        pull.onRefresh();
      }
    }
    this.props.onTouchMove && this.props.onTouchMove(e);
  };
  render() {
    const horizontal = this.props.horizontal === true;
    const NativeScrollView = horizontal ? HScrollViewNativeComponent : VScrollViewNativeComponent;
    const NativeScrollContentView = horizontal
      ? HScrollContentViewNativeComponent
      : VScrollContentViewNativeComponent;
    const contentContainerStyle = [
      horizontal && styles.contentContainerHorizontal,
      this.props.contentContainerStyle,
    ];
    if (__DEV__ && this.props.style !== undefined) {
      const style = flattenStyle(this.props.style);
      const childLayoutProps = ['alignItems', 'justifyContent'].filter(
        (prop) => style && style[prop] !== undefined
      );
      invariant(
        childLayoutProps.length === 0,
        'ScrollView child layout (' +
          JSON.stringify(childLayoutProps) +
          ') must be applied through the contentContainerStyle prop.'
      );
    }
    const contentSizeChangeProps =
      this.props.onContentSizeChange == null
        ? null
        : {
            onLayout: this._handleContentOnLayout,
          };
    const { stickyHeaderIndices } = this.props;
    let children = this.props.children;
    children = React.Children.toArray(children);
    if (stickyHeaderIndices != null && stickyHeaderIndices.length > 0) {
      children = children.map((child, index) => {
        const indexOfIndex = child ? stickyHeaderIndices.indexOf(index) : -1;
        if (indexOfIndex > -1) {
          const key = child.key;
          const nextIndex = stickyHeaderIndices[indexOfIndex + 1];
          const StickyHeaderComponent = this.props.StickyHeaderComponent || ScrollViewStickyHeader;
          return (
            <StickyHeaderComponent
              key={key}
              ref={(ref) => this._setStickyHeaderRef(key, ref)}
              nextHeaderLayoutY={this._headerLayoutYs.get(
                this._getKeyForIndex(nextIndex, children)
              )}
              onLayout={(event) => this._onStickyHeaderLayout(index, event, key)}
              scrollAnimatedValue={this._scrollAnimatedValue}
              inverted={this.props.invertStickyHeaders}
              hiddenOnScroll={this.props.stickyHeaderHiddenOnScroll}
              scrollViewHeight={this.state.layoutHeight}
            >
              {child}
            </StickyHeaderComponent>
          );
        } else {
          return child;
        }
      });
    }
    children = (
      <ScrollViewContext.Provider value={horizontal ? HORIZONTAL : VERTICAL}>
        {children}
      </ScrollViewContext.Provider>
    );
    const hasStickyHeaders = Array.isArray(stickyHeaderIndices) && stickyHeaderIndices.length > 0;
    const preserveChildren =
      this.props.maintainVisibleContentPosition != null ||
      (Platform.OS === 'android' && this.props.snapToAlignment != null);
    const contentContainer = (
      <NativeScrollContentView
        {...contentSizeChangeProps}
        ref={this._innerView.getForwardingRef(this.props.innerViewRef)}
        style={contentContainerStyle}
        removeClippedSubviews={
          Platform.OS === 'android' && hasStickyHeaders ? false : this.props.removeClippedSubviews
        }
        collapsable={false}
        collapsableChildren={!preserveChildren}
      >
        {children}
      </NativeScrollContentView>
    );
    const alwaysBounceHorizontal =
      this.props.alwaysBounceHorizontal !== undefined
        ? this.props.alwaysBounceHorizontal
        : this.props.horizontal;
    const alwaysBounceVertical =
      this.props.alwaysBounceVertical !== undefined
        ? this.props.alwaysBounceVertical
        : !this.props.horizontal;
    const baseStyle = horizontal ? styles.baseHorizontal : styles.baseVertical;
    const { experimental_endDraggingSensitivityMultiplier, ...otherProps } = this.props;
    const props = {
      ...otherProps,
      alwaysBounceHorizontal,
      alwaysBounceVertical,
      style: StyleSheet.compose(baseStyle, this.props.style),
      onContentSizeChange: null,
      onLayout: this._handleLayout,
      onMomentumScrollBegin: this._handleMomentumScrollBegin,
      onMomentumScrollEnd: this._handleMomentumScrollEnd,
      onResponderGrant: this._handleResponderGrant,
      onResponderReject: this._handleResponderReject,
      onResponderRelease: this._handleResponderRelease,
      onResponderTerminationRequest: this._handleResponderTerminationRequest,
      onScrollBeginDrag: this._handleScrollBeginDrag,
      onScrollEndDrag: this._handleScrollEndDrag,
      onScrollShouldSetResponder: this._handleScrollShouldSetResponder,
      onStartShouldSetResponder: this._handleStartShouldSetResponder,
      onStartShouldSetResponderCapture: this._handleStartShouldSetResponderCapture,
      onTouchEnd: this._handleTouchEnd,
      onTouchMove: this._handleTouchMove,
      onTouchStart: this._handleTouchStart,
      onTouchCancel: this._handleTouchCancel,
      onScroll: this._handleScroll,
      endDraggingSensitivityMultiplier: experimental_endDraggingSensitivityMultiplier,
      scrollEventThrottle: hasStickyHeaders ? 1 : this.props.scrollEventThrottle,
      sendMomentumEvents:
        this.props.onMomentumScrollBegin || this.props.onMomentumScrollEnd ? true : false,
      snapToStart: this.props.snapToStart !== false,
      snapToEnd: this.props.snapToEnd !== false,
      pagingEnabled: Platform.select({
        ios:
          this.props.pagingEnabled === true &&
          this.props.snapToInterval == null &&
          this.props.snapToOffsets == null,
        android:
          this.props.pagingEnabled === true ||
          this.props.snapToInterval != null ||
          this.props.snapToOffsets != null,
      }),
    };
    const { decelerationRate } = this.props;
    if (decelerationRate != null) {
      props.decelerationRate = processDecelerationRate(decelerationRate);
    }
    const refreshControl = this.props.refreshControl;
    const scrollViewRef = this._scrollView.getForwardingRef(this.props.scrollViewRef);
    if (refreshControl != null) {
      if (Platform.OS === 'ios') {
        return (
          <NativeScrollView {...props} ref={scrollViewRef}>
            {refreshControl}
            {contentContainer}
          </NativeScrollView>
        );
      } else if (Platform.OS === 'android') {
        const { outer, inner } = splitLayoutProps(flattenStyle(props.style));
        return cloneElement(
          refreshControl,
          {
            style: StyleSheet.compose(baseStyle, outer),
          },
          <NativeScrollView
            {...props}
            nestedScrollEnabled={props.nestedScrollEnabled ?? true}
            style={StyleSheet.compose(baseStyle, inner)}
            ref={scrollViewRef}
          >
            {contentContainer}
          </NativeScrollView>
        );
      }
    }
    return (
      <NativeScrollView {...props} ref={scrollViewRef}>
        {contentContainer}
      </NativeScrollView>
    );
  }
}
const styles = StyleSheet.create({
  baseVertical: {
    flexGrow: 1,
    flexShrink: 1,
    flexDirection: 'column',
    overflow: 'scroll',
  },
  baseHorizontal: {
    flexGrow: 1,
    flexShrink: 1,
    flexDirection: 'row',
    overflow: 'scroll',
  },
  contentContainerHorizontal: {
    flexDirection: 'row',
  },
});
function createRefForwarder(mutator) {
  const state = {
    getForwardingRef: memoize((forwardedRef) => {
      return (nativeInstance) => {
        const publicInstance = nativeInstance == null ? null : mutator(nativeInstance);
        state.nativeInstance = nativeInstance;
        state.publicInstance = publicInstance;
        if (forwardedRef != null) {
          if (typeof forwardedRef === 'function') {
            forwardedRef(publicInstance);
          } else {
            forwardedRef.current = publicInstance;
          }
        }
      };
    }),
    nativeInstance: null,
    publicInstance: null,
  };
  return state;
}
const ScrollViewWrapper = function Wrapper({ ref, ...props }) {
  return ref == null ? <ScrollView {...props} /> : <ScrollView {...props} scrollViewRef={ref} />;
};
ScrollViewWrapper.displayName = 'ScrollView';
ScrollViewWrapper.Context = ScrollViewContext;
export default ScrollViewWrapper;
