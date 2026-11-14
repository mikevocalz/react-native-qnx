#include "react-native-qnx/fabric/QtRnsvgBridge.h"

#include "QtRnsvgBridgeInternal.h"
#include "QtRnsvgBridgeLayoutEvents.h"
#include "react-native-qnx/fabric/QtRnsvgFilterBridge.h"
#include "react-native-qnx/fabric/QtRnsvgImageBridge.h"

#include <RnsvgItems.h>

#include <react/renderer/components/rnsvg/Props.h>
#include <react/renderer/mounting/ShadowView.h>

#include <algorithm>
#include <unordered_map>

namespace rnqnx::fabric {

namespace rn = facebook::react;

namespace {

using namespace rnsvg_bridge;

const std::unordered_map<std::string, Entry>& table() {
  static const std::unordered_map<std::string, Entry> t = [] {
    std::vector<Entry> rows;
    appendShapeEntries(rows);
    appendDefinitionEntries(rows);
    appendTextEntries(rows);
    std::unordered_map<std::string, Entry> built;
    for (const Entry& row : rows) built.emplace(row.name, row);
    return built;
  }();
  return t;
}

// <Svg> root props. `align: "none"` arrives with meetOrSlice 0 (see
// extractViewBox); the model keeps "none" as its own mode.
bool applyRoot(QQuickItem* item, const rn::ShadowView& view) {
  auto* root = qobject_cast<views::RnsvgRootItem*>(item);
  const auto* p = dynamic_cast<const rn::RNSVGSvgViewProps*>(view.props.get());
  if (!root || !p) return false;
  RootProps rp;
  rp.bbWidth = toLength(p->bbWidth);
  rp.bbHeight = toLength(p->bbHeight);
  rp.minX = p->minX;
  rp.minY = p->minY;
  rp.vbWidth = p->vbWidth;
  rp.vbHeight = p->vbHeight;
  if (!rp.hasViewBox()) rp.minX = rp.minY = rp.vbWidth = rp.vbHeight = 0;  // react-native-svg sends none
  if (!p->align.empty()) rp.align = QString::fromStdString(p->align);
  rp.meetOrSlice = p->meetOrSlice >= 0 && p->meetOrSlice <= 2 ? static_cast<int>(p->meetOrSlice) : 0;
  if (rn::isColorMeaningful(p->color)) rp.color = colorFrom(p->color);
  root->setRootProps(rp);
  return true;
}

}  // namespace

std::vector<std::string> rnsvgComponentNames() {
  std::vector<std::string> names{"RNSVGSvgView"};
  for (const auto& [name, entry] : table()) names.push_back(name);
  for (auto& name : rnsvgFilterPrimitiveNames()) names.push_back(std::move(name));
  std::sort(names.begin(), names.end());
  names.erase(std::unique(names.begin(), names.end()), names.end());
  return names;
}

bool isRnsvgComponent(const std::string& name) {
  return name == "RNSVGSvgView" || table().count(name) != 0 || isRnsvgFilterPrimitive(name);
}

void addRnsvgComponentDescriptors(rn::ComponentDescriptorProviderRegistry& registry) {
  rnsvg_bridge::addCoreDescriptors(registry);
  addRnsvgFilterDescriptors(registry);
}

bool applyRnsvgProps(QQuickItem* item, const rn::ShadowView& view) {
  if (view.componentName == nullptr || !view.props) return false;
  const std::string name(view.componentName);
  if (name == "RNSVGSvgView") return applyRoot(item, view);
  if (isRnsvgFilterPrimitive(name)) return applyRnsvgFilterProps(item, view);
  const auto it = table().find(name);
  auto* element = qobject_cast<views::RnsvgElementItem*>(item);
  if (it == table().end() || !element) return false;
  ElementProps next;
  next.kind = it->second.kind;
  if (!it->second.convert(*view.props, next, element->props())) return false;
  element->setProps(std::move(next));
  // The emitter changes on a remount only, but setting it is cheap and keeps
  // the reporter from holding one that has been replaced.
  if (it->second.kind == Kind::Image) attachImageLoading(element, view);
  if (it->second.emitsSvgLayout && view.eventEmitter) SvgLayoutReporter::attach(element, view.eventEmitter);
  return true;
}

}  // namespace rnqnx::fabric
