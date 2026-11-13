// react-native-svg card; the drawing lives in svg/FiltersCard.js.
import { svgAdvancedCard } from '../../svg/advancedCard';

export default svgAdvancedCard({
  id: 'svg-filters',
  title: 'Filters',
  order: 18,
  load: () => require('../../svg/FiltersCard'),
});
