// Registration stays separate from the component/context module so route
// changes can refresh without repeating application registration.
import { AppRegistry } from 'react-native';
import App from './components/router-root';

AppRegistry.registerComponent('__COMPONENT_NAME__', () => App);
