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
      options,
    });
  });
  return tabs;
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

module.exports = { buildTabs, tintsFor, resolveTabBarLayout, resolveTabBarEdge, isRail, labelFor };
