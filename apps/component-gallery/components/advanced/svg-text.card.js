// react-native-svg card; the drawing lives in svg/TextCard.js.
import { svgAdvancedCard } from '../../svg/advancedCard';

export default svgAdvancedCard({
  id: 'svg-text',
  title: 'Text',
  order: 13,
  load: () => require('../../svg/TextCard'),
});
