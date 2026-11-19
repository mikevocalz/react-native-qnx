// Shared values, withSpring, withTiming, frame callback
import { motionCard } from '../../motion/lazyCard';

export default motionCard({
  id: 'reanimated-basics',
  title: 'Reanimated 4',
  detail: 'Shared values, withSpring, withTiming, frame callback',
  order: 21,
  needsWorklets: true,
  load: () => require('../../motion/ReanimatedCard.js').ReanimatedCard,
});
