// Number parsing shared by the overlay's extractors. react-native-svg's xml
// parser turns every numeric-looking attribute into a number and leaves lists
// as strings, so each prop can arrive as a number, a string or an array.

export type NumberProp = string | number;
export type NumberArray = NumberProp[] | NumberProp;

const separator = /[\s,]+/;

/** A finite number, or `fallback` for NaN, Infinity or a non-numeric value. */
export function toNumber(value: unknown, fallback: number): number {
  if (value === undefined || value === null || value === '') return fallback;
  const parsed = typeof value === 'number' ? value : parseFloat(String(value));
  return Number.isFinite(parsed) ? parsed : fallback;
}

/** `"1 2 3"`, `[1, '2']` or `3` as numbers. Entries that are not numbers are dropped. */
export function toNumberList(value: NumberArray | undefined | null): number[] {
  if (value === undefined || value === null) return [];
  const items = Array.isArray(value)
    ? value
    : typeof value === 'number'
      ? [value]
      : String(value).trim().split(separator);
  const out: number[] = [];
  for (const item of items) {
    const parsed = typeof item === 'number' ? item : parseFloat(item);
    if (Number.isFinite(parsed)) out.push(parsed);
  }
  return out;
}

/**
 * Attributes that take one number or two (`radius`, `baseFrequency`,
 * `stdDeviation`, `order`): one value applies to both axes.
 */
export function toPair(value: NumberArray | undefined | null, fallback: number): [number, number] {
  const list = toNumberList(value);
  if (list.length === 0) return [fallback, fallback];
  return [list[0], list.length > 1 ? list[1] : list[0]];
}

/** The value if it is one of `allowed`, else undefined. */
export function oneOf<T extends string>(value: unknown, allowed: readonly T[]): T | undefined {
  return typeof value === 'string' && (allowed as readonly string[]).includes(value)
    ? (value as T)
    : undefined;
}
