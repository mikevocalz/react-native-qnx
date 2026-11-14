// react-native-svg card; the drawing lives in svg/PatternsCard.js.
import { svgAdvancedCard } from '../../svg/advancedCard';

export default svgAdvancedCard({
  id: 'svg-patterns',
  title: 'Pattern, symbol and marker',
  order: 15,
  load: () => require('../../svg/PatternsCard'),
});
