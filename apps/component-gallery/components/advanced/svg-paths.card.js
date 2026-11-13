// react-native-svg card; the drawing lives in svg/PathsCard.js.
import { svgAdvancedCard } from '../../svg/advancedCard';

export default svgAdvancedCard({
  id: 'svg-paths',
  title: 'Paths and shapes',
  order: 10,
  load: () => require('../../svg/PathsCard'),
});
