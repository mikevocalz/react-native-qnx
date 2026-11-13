// Screen options that make expo-router's <Tabs> follow the window: a bottom
// bar on narrow windows, a left rail from `breakpoint` px up. Pass the
// measured width (an onLayout of the layout's root View, so it tracks live
// resizes of the host window).
//
//   <Tabs screenOptions={qnxTabsScreenOptions({ width })}>
const { resolveTabBarLayout } = require('./tabBarModel');

function qnxTabsScreenOptions({ width, layout = 'auto', breakpoint = 700, ...rest } = {}) {
  const resolved = resolveTabBarLayout(layout, width ?? 0, breakpoint);
  return {
    tabBarPosition: resolved === 'side' ? 'left' : 'bottom',
    qnxTabBarLayout: layout,
    qnxSideBreakpoint: breakpoint,
    animation: 'none',
    ...rest,
  };
}

module.exports = { qnxTabsScreenOptions };
