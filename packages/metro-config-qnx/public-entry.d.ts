import type { InputConfigT } from 'metro-config';

/** Opt in to the documented, partial React Native public export surface.
 * Other platform requests retain the supplied resolver behavior. */
declare function withPublicEntry(config: InputConfigT, platform?: 'qnx' | 'marathon'): InputConfigT;
export = withPublicEntry;
