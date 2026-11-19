import QtQuick
import Rnqnx.ExpoImage

// ViewManagerAdapter_ExpoImage: the C++ ExpoImageItem loads, decodes and
// paints the picture. This file only gives it the Qt accessibility attached
// properties, which exist in QML alone; the Fabric bridge fills the
// `accessibility*` properties from the view's props.
ExpoImageItem {
    Accessible.ignored: !accessible
    Accessible.role: roleFor(accessibilityRole)
    Accessible.name: accessibilityLabel
    Accessible.description: accessibilityHint
    // Qt derives the accessible "disabled" state from `enabled`.
    enabled: !accessibilityStateDisabled
    Accessible.selected: accessibilityStateSelected
    Accessible.checkable: accessibilityRole === "checkbox" || accessibilityRole === "switch"
                          || accessibilityRole === "radio"
    Accessible.checked: accessibilityStateChecked

    function roleFor(role) {
        switch (role) {
        case "button": return Accessible.Button;
        case "link": return Accessible.Link;
        case "text": return Accessible.StaticText;
        case "header": return Accessible.Heading;
        case "checkbox": return Accessible.CheckBox;
        case "radio": return Accessible.RadioButton;
        case "switch": return Accessible.Switch;
        case "imagebutton": return Accessible.Button;
        default: return Accessible.Graphic;
        }
    }
}
