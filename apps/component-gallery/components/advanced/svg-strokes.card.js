// react-native-svg card; the drawing lives in svg/StrokesCard.js.
import { svgAdvancedCard } from '../../svg/advancedCard';

export default svgAdvancedCard({
  id: 'svg-strokes',
  title: 'Fill rules, strokes and gradient spread',
  order: 16,
  load: () => require('../../svg/StrokesCard'),
});
