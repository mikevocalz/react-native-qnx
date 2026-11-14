import type { ReactNode } from 'react';
import FeDistantLight from 'react-native-svg/src/elements/filters/FeDistantLight';
import FePointLight from 'react-native-svg/src/elements/filters/FePointLight';
import FeSpotLight from 'react-native-svg/src/elements/filters/FeSpotLight';
import { elementsOf, isElementOf } from './children';
import { toNumber } from './numbers';

/** The `light` struct of RNSVGFeDiffuseLighting and RNSVGFeSpecularLighting. */
export interface LightStruct {
  kind: 'none' | 'distant' | 'point' | 'spot';
  azimuth?: number;
  elevation?: number;
  x?: number;
  y?: number;
  z?: number;
  pointsAtX?: number;
  pointsAtY?: number;
  pointsAtZ?: number;
  specularExponent?: number;
  limitingConeAngle?: number;
  hasLimitingCone?: boolean;
}

type LightProps = Record<string, unknown>;

/**
 * The first light source child of a lighting primitive. A lighting element
 * without one is an error in SVG; it is sent as kind `none` so the native side
 * can decide, rather than being given a made-up light here.
 */
export function extractLightSource(children: ReactNode): LightStruct {
  for (const child of elementsOf(children)) {
    const props = child.props as LightProps;
    if (isElementOf(child, [FeDistantLight])) {
      return {
        kind: 'distant',
        azimuth: toNumber(props.azimuth, 0),
        elevation: toNumber(props.elevation, 0),
      };
    }
    if (isElementOf(child, [FePointLight])) {
      return {
        kind: 'point',
        x: toNumber(props.x, 0),
        y: toNumber(props.y, 0),
        z: toNumber(props.z, 0),
      };
    }
    if (isElementOf(child, [FeSpotLight])) {
      const cone = toNumber(props.limitingConeAngle, NaN);
      return {
        kind: 'spot',
        x: toNumber(props.x, 0),
        y: toNumber(props.y, 0),
        z: toNumber(props.z, 0),
        pointsAtX: toNumber(props.pointsAtX, 0),
        pointsAtY: toNumber(props.pointsAtY, 0),
        pointsAtZ: toNumber(props.pointsAtZ, 0),
        specularExponent: toNumber(props.specularExponent, 1),
        limitingConeAngle: Number.isNaN(cone) ? 0 : cone,
        hasLimitingCone: !Number.isNaN(cone),
      };
    }
  }
  return { kind: 'none' };
}
