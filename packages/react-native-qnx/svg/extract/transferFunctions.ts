import { Children, isValidElement } from 'react';
import type { ReactNode } from 'react';
import {
  FeFuncA,
  FeFuncB,
  FeFuncG,
  FeFuncR,
} from 'react-native-svg/src/elements/filters/FeComponentTransferFunction';
import { oneOf, toNumber, toNumberList } from './numbers';

const functionTypes = ['identity', 'table', 'discrete', 'linear', 'gamma'] as const;

/** One struct of RNSVGFeComponentTransfer: `funcR`, `funcG`, `funcB` or `funcA`. */
export interface TransferStruct {
  type: string;
  tableValues: number[];
  slope: number;
  intercept: number;
  amplitude: number;
  exponent: number;
  offset: number;
}

export interface TransferFunctions {
  funcR?: TransferStruct;
  funcG?: TransferStruct;
  funcB?: TransferStruct;
  funcA?: TransferStruct;
}

const channelOf = new Map<unknown, keyof TransferFunctions>([
  [FeFuncR, 'funcR'],
  [FeFuncG, 'funcG'],
  [FeFuncB, 'funcB'],
  [FeFuncA, 'funcA'],
]);

function toStruct(props: Record<string, unknown>): TransferStruct {
  return {
    type: oneOf(props.type, functionTypes) ?? 'identity',
    tableValues: toNumberList(props.tableValues as string | number | undefined),
    slope: toNumber(props.slope, 1),
    intercept: toNumber(props.intercept, 0),
    amplitude: toNumber(props.amplitude, 1),
    exponent: toNumber(props.exponent, 1),
    offset: toNumber(props.offset, 0),
  };
}

/**
 * Reads the feFuncR/G/B/A children. The package's elements render nothing, so
 * the primitive carries their attributes itself. When a channel has several
 * children the last one wins, as in browsers.
 */
export function extractTransferFunctions(children: ReactNode): TransferFunctions {
  const out: TransferFunctions = {};
  for (const child of Children.toArray(children)) {
    if (!isValidElement(child)) continue;
    const channel = channelOf.get(child.type);
    if (channel) out[channel] = toStruct(child.props as Record<string, unknown>);
  }
  return out;
}
