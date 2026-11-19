import { svgAdvancedCard } from '../../svg/advancedCard';

export default svgAdvancedCard({
  id: 'svg-foreign-object',
  title: 'ForeignObject live children',
  order: 21,
  load: () => require('../../svg/ForeignObjectCard'),
});
