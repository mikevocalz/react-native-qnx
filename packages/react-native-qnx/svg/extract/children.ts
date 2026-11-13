import { Children, Fragment, isValidElement } from 'react';
import type { ReactElement, ReactNode } from 'react';

/** The name a component type goes by, for elements made by another copy of React or a wrapper. */
function nameOf(type: unknown): string | undefined {
  return typeof type === 'function' || (typeof type === 'object' && type !== null)
    ? (type as { displayName?: string }).displayName
    : undefined;
}

/**
 * Every element under `children`, looking through fragments, in document order.
 * Types are matched by identity or by displayName, so a subclass or a
 * re-exported element still counts.
 */
export function elementsOf(children: ReactNode, out: ReactElement[] = []): ReactElement[] {
  for (const child of Children.toArray(children)) {
    if (!isValidElement(child)) continue;
    if (child.type === Fragment) {
      elementsOf((child.props as { children?: ReactNode }).children, out);
    } else {
      out.push(child);
    }
  }
  return out;
}

/** Whether `element` is one of `types`, by identity or displayName. */
export function isElementOf(element: ReactElement, types: readonly { displayName?: string }[]) {
  return types.some(
    (type) => element.type === type || (type.displayName && nameOf(element.type) === type.displayName)
  );
}
