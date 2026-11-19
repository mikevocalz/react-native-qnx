// react-native-svg card; the drawing lives in svg/XmlCard.js.
import { svgAdvancedCard } from '../../svg/advancedCard';

export default svgAdvancedCard({
  id: 'svg-xml',
  title: 'SvgXml, SvgCss and parse',
  order: 20,
  load: () => require('../../svg/XmlCard'),
});
