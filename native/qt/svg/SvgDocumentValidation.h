#pragma once

#include <QByteArray>
#include <QString>
#include <optional>

namespace rnqnx::views::detail {

// No Qt SVG parser, filesystem or network is touched until this bounded
// allow-list check succeeds. Returns a diagnostic for a rejected document.
std::optional<QString> validateSvgDocument(const QByteArray& utf8Xml);

}  // namespace rnqnx::views::detail
