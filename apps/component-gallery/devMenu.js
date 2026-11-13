// Developer-menu entries (F1 or Ctrl+D) that drive the router from the
// keyboard, for runs where only keys can be injected: the drawer toggle, each
// tab and each drawer page. DevSettings.addMenuItem has no remove, so the
// entries are added once and call whatever the mounted screens last
// registered. A no-op in release builds.
import { DevSettings } from 'react-native';

const actions = { toggleDrawer: null, navigate: null };
let installed = false;

export function registerDevMenuActions(next) {
  Object.assign(actions, next);
  if (installed || !__DEV__) return;
  installed = true;
  DevSettings.addMenuItem('Toggle drawer', () => actions.toggleDrawer?.());
  for (const [label, href] of [
    ['Components tab', '/'],
    ['Advanced tab', '/advanced'],
    ['Documents tab', '/documents'],
    ['Controls tab', '/controls'],
    ['More tab', '/more'],
    ['Components page', '/components'],
    ['Media page', '/media'],
    ['Settings page', '/settings'],
    ['About page', '/about'],
  ]) {
    DevSettings.addMenuItem('Go to ' + label, () => actions.navigate?.(href));
  }
}
