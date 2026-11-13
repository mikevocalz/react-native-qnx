// react-native-svg card; the drawing lives in svg/ClipMaskCard.js.
import { svgAdvancedCard } from '../../svg/advancedCard';

export default svgAdvancedCard({
  id: 'svg-clip-mask',
  title: 'Clip, mask and use',
  order: 12,
  load: () => require('../../svg/ClipMaskCard'),
});
