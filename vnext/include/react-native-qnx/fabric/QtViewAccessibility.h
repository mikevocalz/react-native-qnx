#pragma once
#include <QString>
class QQuickItem;
namespace rnqnx::fabric {
struct ViewStyle;
// Generic View semantics only. Does not wire RN accessibility actions or an OS
// transport. Qt may advertise default actions for mapped roles. RN disabled
// metadata does not change native input enablement or override its queried state.
bool applyViewAccessibility(QQuickItem& item, const ViewStyle& style, QString& error);
}
