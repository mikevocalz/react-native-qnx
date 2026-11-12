#include "SvgPathValidation.h"
#include "SvgDocument.h"

#include <QRegularExpression>
#include <cmath>

namespace rnqnx::views::detail {
namespace {
bool isSpace(QChar value) {
  return value == ' ' || value == '\t' || value == '\n' || value == '\r';
}

int arity(QChar command) {
  switch (command.toUpper().toLatin1()) {
    case 'M': case 'L': case 'T': return 2;
    case 'H': case 'V': return 1;
    case 'C': return 6;
    case 'S': case 'Q': return 4;
    case 'A': return 7;
    case 'Z': return 0;
    default: return -1;
  }
}
}  // namespace

bool isSupportedSvgPath(const QString& path) {
  static const QRegularExpression number("[-+]?(?:[0-9]+(?:\\.[0-9]*)?|\\.[0-9]+)(?:[eE][-+]?[0-9]+)?");
  qsizetype position = 0;
  int tokens = 0;
  bool first = true;
  const auto skipSpaces = [&] { while (position < path.size() && isSpace(path[position])) ++position; };

  while (position < path.size()) {
    skipSpaces();
    if (position == path.size()) break;
    const QChar command = path[position++];
    const int count = arity(command);
    if (count < 0 || (first && command.toUpper() != 'M') || ++tokens > 4096) return false;
    first = false;
    if (count == 0) continue;
    int groups = 0;
    bool hasNumber = false;
    while (true) {
      skipSpaces();
      if (position == path.size() || path[position].isLetter()) break;
      for (int argument = 0; argument < count; ++argument) {
        skipSpaces();
        if (position < path.size() && path[position] == ',') {
          if (!hasNumber) return false;
          ++position;
          skipSpaces();
        }
        if (position == path.size() || ++tokens > 4096) return false;
        if (command.toUpper() == 'A' && (argument == 3 || argument == 4)) {
          if (path[position] != '0' && path[position] != '1') return false;
          ++position;
        } else {
          const auto match = number.match(path, position, QRegularExpression::NormalMatch,
                                          QRegularExpression::AnchorAtOffsetMatchOption);
          if (!match.hasMatch()) return false;
          bool ok = false;
          const double value = match.captured().toDouble(&ok);
          if (!ok || !std::isfinite(value) || std::abs(value) > kSvgMaxCoordinate ||
              (command.toUpper() == 'A' && argument < 2 && value < 0)) return false;
          position = match.capturedEnd();
        }
        hasNumber = true;
      }
      ++groups;
    }
    if (groups == 0) return false;
  }
  return !first;
}

}  // namespace rnqnx::views::detail
