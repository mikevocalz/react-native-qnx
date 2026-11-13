// react-native-svg card; the drawing lives in svg/TextLayoutCard.js.
import { svgAdvancedCard } from '../../svg/advancedCard';

export default svgAdvancedCard({
  id: 'svg-text-layout',
  title: 'Text layout',
  order: 17,
  load: () => require('../../svg/TextLayoutCard'),
});
