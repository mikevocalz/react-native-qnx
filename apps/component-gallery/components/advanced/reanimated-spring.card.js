// Gesture Handler 3 pan with a Reanimated spring back
import { motionCard } from '../../motion/lazyCard';

export default motionCard({
  id: 'reanimated-spring',
  title: 'Draggable spring card',
  detail: 'Gesture Handler 3 pan with a Reanimated spring back',
  order: 22,
  needsWorklets: true,
  load: () => require('../../motion/SpringCard.js').SpringCard,
});
