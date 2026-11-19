// @legendapp/list 3.7.2, 5,000 variable-height rows
import { motionCard } from '../../motion/lazyCard';

export default motionCard({
  id: 'legend-list',
  title: 'Legend List',
  detail: '@legendapp/list 3.7.2, 5,000 variable-height rows',
  order: 61,
  needsWorklets: false,
  load: () => require('../../motion/LegendListCard.js').LegendListCard,
});
