// react-native-svg card; the drawing lives in svg/ImageCard.js.
import { svgAdvancedCard } from '../../svg/advancedCard';

export default svgAdvancedCard({
  id: 'svg-image',
  title: 'Image',
  order: 19,
  load: () => require('../../svg/ImageCard'),
});
