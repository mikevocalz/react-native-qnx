/**
 * Pull-to-refresh gesture state for the QNX `ScrollView`.
 *
 * React Native has two ways to pull to refresh and neither applies here. On iOS
 * the `RefreshControl` is a native view inside the scroll view, and on Android
 * a native swipe layout wraps it. Both need a native overscroll, and the Qt
 * Quick scroll item stops at its bounds, so a drag at the top moves nothing and
 * reports no offset. What the host does deliver is touch events, which carry the
 * page position of the finger, and scroll events, which say whether the content
 * has moved off the top.
 *
 * One tracker per `ScrollView`. A pull is a single finger that goes down while
 * the content is at the top and then travels `threshold` points downward without
 * the content scrolling. It fires once per gesture, as soon as it crosses the
 * threshold rather than on release, because the scroll item may take over the
 * touch and end it with a cancel.
 */

'use strict';

// About the distance iOS asks for before it starts a refresh.
const PULL_THRESHOLD = 72;

/**
 * @param {number} [threshold] Downward travel, in points, that counts as a pull.
 */
function createPullToRefresh(threshold = PULL_THRESHOLD) {
  /** Page Y where the pull began, or null when no pull is in progress. */
  let startY = null;
  let fired = false;

  return {
    /**
     * A finger went down.
     * @param {number} pageY
     * @param {number} scrollY Current vertical content offset.
     * @param {number} touchCount Fingers down, including this one.
     * @param {boolean} enabled Whether a refresh may start now.
     */
    start(pageY, scrollY, touchCount, enabled) {
      fired = false;
      startY = enabled && touchCount === 1 && scrollY <= 0 && Number.isFinite(pageY) ? pageY : null;
    },

    /** The content scrolled: a drag that moved it did not start from the top. */
    scrolled(scrollY) {
      if (scrollY > 0) startY = null;
    },

    /**
     * A finger moved.
     * @param {number} pageY
     * @param {number} touchCount Fingers down.
     * @returns {boolean} True once, when the pull crosses the threshold.
     */
    move(pageY, touchCount) {
      if (startY === null || fired) return false;
      if (touchCount !== 1) {
        startY = null;
        return false;
      }
      if (pageY - startY < threshold) return false;
      fired = true;
      return true;
    },

    /** The gesture ended or was cancelled. */
    end() {
      startY = null;
      fired = false;
    },
  };
}

module.exports = { createPullToRefresh, PULL_THRESHOLD };
