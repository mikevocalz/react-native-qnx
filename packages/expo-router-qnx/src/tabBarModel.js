// Turns expo-router's tab bar inputs (navigation state and screen options)
// into plain data that both the native QtTabBar and the JavaScript fallback
// draw from.

const DEFAULT_ACTIVE = '#79e8dc';
const DEFAULT_INACTIVE = '#9dafc2';
const DEFAULT_BACKGROUND = '#102338';
const DEFAULT_BADGE = '#e5484d';
const DEFAULT_BADGE_TEXT = '#ffffff';

/**
 * The edge a tab bar sits on, from the `tabBarPosition` screen option: `top`,
 * `bottom`, `left` (start side) or `right` (end side). Without one, `layout`
 * decides between a bottom bar and a left rail.
 */
function resolveTabBarEdge(position, layout) {
  if (position === 'top' || position === 'bottom' || position === 'left' || position === 'right') {
    return position;
  }
  return layout === 'side' ? 'left' : 'bottom';
}

/** Whether a bar on \`edge\` draws as a vertical rail. */
function isRail(edge) {
  return edge === 'left' || edge === 'right';
}

/** `auto` follows the window width; `bottom` and `side` are fixed. */
function resolveTabBarLayout(layout, width, breakpoint = 700) {
  if (layout === 'bottom' || layout === 'side') return layout;
  return width >= breakpoint ? 'side' : 'bottom';
}

function labelFor(options, routeName) {
  if (typeof options.tabBarLabel === 'string') return options.tabBarLabel;
  if (typeof options.title === 'string') return options.title;
  return routeName;
}

/**
 * @param {{ routes: any[], index: number }} state
 * @param {Record<string, { options: any }>} descriptors
 */
function buildTabs(state, descriptors) {
  const tabs = [];
  state.routes.forEach((route) => {
    const descriptor = descriptors[route.key];
    if (!descriptor) return;
    const options = descriptor.options ?? {};
    // href: null and `hidden` both hide a tab.
    if (options.hidden === true) return;
    const badge = options.tabBarBadge;
    tabs.push({
      key: route.key,
      name: route.name,
      title: labelFor(options, route.name),
      // qnxIcon: a glyph (one or two characters from the system font) or an
      // image URI. tabBarIcon is a React element factory and cannot cross to
      // native, so the native bar only shows qnxIcon.
      icon: typeof options.qnxIcon === 'string' ? options.qnxIcon : '',
      badge: badge === undefined || badge === null ? '' : String(badge),
      accessibilityLabel:
        typeof options.tabBarAccessibilityLabel === 'string'
          ? options.tabBarAccessibilityLabel
          : labelFor(options, route.name),
      // Per-tab overrides of the bar-wide values (the JavaScript bar draws them).
      activeTint: options.tabBarActiveTintColor,
      inactiveTint: options.tabBarInactiveTintColor,
      activeBackground: options.tabBarActiveBackgroundColor,
      inactiveBackground: options.tabBarInactiveBackgroundColor,
      testID: options.tabBarButtonTestID,
      options,
    });
  });
  return tabs;
}

function flatStyle(style) {
  return Array.isArray(style)
    ? Object.assign({}, ...style.filter(Boolean).map(flatStyle))
    : (style ?? {});
}

// What the native bar can draw. Any other tab bar option sends the bar to
// JsTabBar, which draws the whole set (see BottomTabBar).
const NATIVE_BAR_STYLE_KEYS = new Set(['backgroundColor', 'height', 'display']);
const NATIVE_BADGE_STYLE_KEYS = new Set(['backgroundColor', 'color']);
const JS_ONLY_TAB_OPTIONS = [
  'tabBarButton',
  'tabBarItemStyle',
  'tabBarLabelStyle',
  'tabBarIconStyle',
  'tabBarAllowFontScaling',
  'tabBarActiveBackgroundColor',
  'tabBarInactiveBackgroundColor',
  'tabBarButtonTestID',
];

/**
 * Whether the bar needs JsTabBar: a React element (tabBarIcon without qnxIcon,
 * tabBarBackground, tabBarButton) or a style the native bar does not draw.
 */
function needsJsTabBar(tabs, focusedOptions) {
  if (tabs.some((tab) => typeof tab.options.tabBarIcon === 'function' && !tab.icon)) return true;
  const bar = focusedOptions;
  if (bar.tabBarBackground !== undefined) return true;
  if (bar.tabBarVariant === 'material') return true;
  if (bar.tabBarLabelPosition === 'beside-icon') return true;
  const style = flatStyle(bar.tabBarStyle);
  if (Object.keys(style).some((key) => !NATIVE_BAR_STYLE_KEYS.has(key))) return true;
  return tabs.some((tab) => {
    if (JS_ONLY_TAB_OPTIONS.some((name) => tab.options[name] !== undefined)) return true;
    // The native bar takes one tint pair for the bar, from the focused tab.
    if (tab.activeTint !== bar.tabBarActiveTintColor) return true;
    if (tab.inactiveTint !== bar.tabBarInactiveTintColor) return true;
    const badge = flatStyle(tab.options.tabBarBadgeStyle);
    return Object.keys(badge).some((key) => !NATIVE_BADGE_STYLE_KEYS.has(key));
  });
}

function tintsFor(options) {
  const style = Array.isArray(options.tabBarStyle)
    ? Object.assign({}, ...options.tabBarStyle.filter(Boolean))
    : (options.tabBarStyle ?? {});
  const badge = Array.isArray(options.tabBarBadgeStyle)
    ? Object.assign({}, ...options.tabBarBadgeStyle.filter(Boolean))
    : (options.tabBarBadgeStyle ?? {});
  return {
    active: options.tabBarActiveTintColor ?? DEFAULT_ACTIVE,
    inactive: options.tabBarInactiveTintColor ?? DEFAULT_INACTIVE,
    background: style.backgroundColor ?? DEFAULT_BACKGROUND,
    badge: badge.backgroundColor ?? DEFAULT_BADGE,
    badgeText: badge.color ?? DEFAULT_BADGE_TEXT,
  };
}

module.exports = {
  buildTabs,
  flatStyle,
  needsJsTabBar,
  tintsFor,
  resolveTabBarLayout,
  resolveTabBarEdge,
  isRail,
  labelFor,
};
