// react-native-svg card; the drawing lives in svg/PressableCard.js.
import { svgAdvancedCard } from '../../svg/advancedCard';

export default svgAdvancedCard({
  id: 'svg-touch',
  title: 'Touch targets and updates',
  order: 14,
  load: () => require('../../svg/PressableCard'),
});
