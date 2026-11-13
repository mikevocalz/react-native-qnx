// Drag left to delete, spring back below the threshold
import { motionCard } from '../../motion/lazyCard';

export default motionCard({
  id: 'swipe-rows',
  title: 'Swipeable list items',
  detail: 'Drag left to delete, spring back below the threshold',
  order: 23,
  needsWorklets: true,
  load: () => require('../../motion/SwipeRow.js').SwipeRow,
});
