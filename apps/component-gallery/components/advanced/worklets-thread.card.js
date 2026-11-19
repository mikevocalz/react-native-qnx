// react-native-worklets 0.13 on its own UI-worklet thread
import { motionCard } from '../../motion/lazyCard';

export default motionCard({
  id: 'worklets-thread',
  title: 'Worklets UI thread',
  detail: 'react-native-worklets 0.13 on its own UI-worklet thread',
  order: 20,
  needsWorklets: true,
  load: () => require('../../motion/WorkletsProof.js').WorkletsProof,
});
