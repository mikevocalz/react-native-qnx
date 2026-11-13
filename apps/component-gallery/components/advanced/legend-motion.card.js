// @legendapp/motion 2.5.3 over core Animated
import { motionCard } from '../../motion/lazyCard';

export default motionCard({
  id: 'legend-motion',
  title: 'Legend Motion',
  detail: '@legendapp/motion 2.5.3 over core Animated',
  order: 60,
  needsWorklets: false,
  load: () => require('../../motion/LegendMotionCard.js').LegendMotionCard,
});
