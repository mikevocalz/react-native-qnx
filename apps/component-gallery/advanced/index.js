// Cards for the gallery's "Advanced" tab. One file per card; add a card by
// requiring it here. Each entry: { id, title, component }.
const { WorkletsProof } = require('./WorkletsProof.js');
const { LegendMotionCard } = require('./LegendMotionCard.js');
const { LegendListCard } = require('./LegendListCard.js');
const { ReanimatedCard } = require('./ReanimatedCard.js');
const { SpringCard } = require('./SpringCard.js');
const { SwipeRow } = require('./SwipeRow.js');

module.exports = {
  CARDS: [
    { id: 'worklets', title: 'Worklets', component: WorkletsProof },
    { id: 'reanimated', title: 'Reanimated', component: ReanimatedCard },
    { id: 'spring-card', title: 'Spring card', component: SpringCard },
    { id: 'swipe-row', title: 'Swipeable rows', component: SwipeRow },
    { id: 'legend-motion', title: 'Legend Motion', component: LegendMotionCard },
    { id: 'legend-list', title: 'Legend List', component: LegendListCard },
  ],
};
