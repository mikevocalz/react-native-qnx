#pragma once

// The props one element sees: its `style` declarations overlaid by its
// attributes, which is propsAndStyles() in src/lib/extract/extractProps.ts
// (an explicit prop wins over the same name in `style`).

#include "XmlDocument.h"

#include <QSet>

namespace rnqnx::views::rnsvg::testing {

class PropBag {
 public:
  explicit PropBag(const XmlNode& node) : PropBag(node, nullptr) {}
  // Only the names in `allowed` when it is given.
  PropBag(const XmlNode& node, const QSet<QString>* allowed) {
    auto keep = [&](const QString& name) { return !allowed || allowed->contains(name); };
    for (const auto& [name, value] : node.style) {
      if (keep(name)) values_.insert(name, value);
    }
    for (auto it = node.attrs.constBegin(); it != node.attrs.constEnd(); ++it) {
      if (it.key() != QLatin1String("style") && keep(it.key())) values_.insert(it.key(), it.value());
    }
  }
  bool has(const QString& name) const { return values_.contains(name); }
  QString get(const QString& name) const { return values_.value(name); }
  // First of several names that is present.
  QString first(std::initializer_list<const char*> names) const {
    for (const char* n : names) {
      if (has(QLatin1String(n))) return get(QLatin1String(n));
    }
    return {};
  }

 private:
  QHash<QString, QString> values_;
};

}  // namespace rnqnx::views::rnsvg::testing
