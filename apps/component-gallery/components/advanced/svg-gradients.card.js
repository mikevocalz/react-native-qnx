// react-native-svg card; the drawing lives in svg/GradientsCard.js.
import { svgAdvancedCard } from '../../svg/advancedCard';

export default svgAdvancedCard({
  id: 'svg-gradients',
  title: 'Gradients',
  order: 11,
  load: () => require('../../svg/GradientsCard'),
});
