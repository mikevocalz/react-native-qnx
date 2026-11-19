#include "SvgCss.h"

#include <QRegularExpression>

#include <algorithm>

namespace rnqnx::views::rnsvg::testing {
namespace {

constexpr int kMaxRules = 20000;

struct Declaration {
  QString name;
  QString value;
  bool important{false};
};

// One compound selector: tag, id and classes that must all match.
struct Compound {
  QString tag;  // empty or "*" matches any
  QString id;
  QStringList classes;
  enum class Link { None, Descendant, Child } link{Link::None};  // how it relates to the compound on its left
};

struct Rule {
  std::vector<Compound> chain;  // leftmost first
  std::vector<Declaration> declarations;
  int ids{0}, classes{0}, tags{0};
  int order{0};
};

QString stripComments(QString css) {
  static const QRegularExpression comment(QStringLiteral("/\\*.*?\\*/"), QRegularExpression::DotMatchesEverythingOption);
  return css.remove(comment);
}

bool parseSelector(const QString& text, Rule& rule) {
  if (text.contains(QLatin1Char(':')) || text.contains(QLatin1Char('[')) || text.contains(QLatin1Char('+')) ||
      text.contains(QLatin1Char('~'))) {
    return false;
  }
  Compound::Link pending = Compound::Link::None;
  const QString spaced = QString(text).replace(QLatin1Char('>'), QStringLiteral(" > "));
  const QStringList tokens = spaced.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
  for (const QString& token : tokens) {
    if (token == QLatin1String(">")) { pending = Compound::Link::Child; continue; }
    Compound c;
    c.link = rule.chain.empty() ? Compound::Link::None : (pending == Compound::Link::Child ? Compound::Link::Child : Compound::Link::Descendant);
    pending = Compound::Link::None;
    qsizetype i = 0;
    auto ident = [&]() {
      const qsizetype s = i;
      while (i < token.size() && token[i] != QLatin1Char('.') && token[i] != QLatin1Char('#')) ++i;
      return token.mid(s, i - s);
    };
    c.tag = ident();
    while (i < token.size()) {
      const QChar kind = token[i++];
      const QString name = ident();
      if (name.isEmpty()) return false;
      if (kind == QLatin1Char('#')) { c.id = name; ++rule.ids; }
      else { c.classes.push_back(name); ++rule.classes; }
    }
    if (!c.tag.isEmpty() && c.tag != QLatin1String("*")) ++rule.tags;
    rule.chain.push_back(std::move(c));
  }
  return !rule.chain.empty();
}

std::vector<Declaration> parseDeclarations(const QString& body) {
  std::vector<Declaration> out;
  for (const QString& d : body.split(QLatin1Char(';'))) {
    const qsizetype colon = d.indexOf(QLatin1Char(':'));
    if (colon <= 0) continue;
    Declaration decl;
    decl.name = d.left(colon).trimmed();
    decl.value = d.mid(colon + 1).trimmed();
    if (decl.value.endsWith(QLatin1String("!important"))) {
      decl.important = true;
      decl.value = decl.value.chopped(10).trimmed();
    }
    if (!decl.name.isEmpty() && !decl.value.isEmpty()) out.push_back(std::move(decl));
  }
  return out;
}

void collectRules(const QString& css, std::vector<Rule>& rules, int& order) {
  QString rest = stripComments(css);
  qsizetype pos = 0;
  while (pos < rest.size() && int(rules.size()) < kMaxRules) {
    const qsizetype open = rest.indexOf(QLatin1Char('{'), pos);
    if (open < 0) break;
    const QString selectorText = rest.mid(pos, open - pos).trimmed();
    // Find the matching close, so @media blocks are skipped whole.
    int depth = 1;
    qsizetype close = open + 1;
    while (close < rest.size() && depth > 0) {
      if (rest[close] == QLatin1Char('{')) ++depth;
      else if (rest[close] == QLatin1Char('}')) --depth;
      ++close;
    }
    pos = close;
    if (selectorText.startsWith(QLatin1Char('@'))) continue;
    const auto declarations = parseDeclarations(rest.mid(open + 1, close - open - 2));
    for (const QString& sel : selectorText.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
      Rule rule;
      if (!parseSelector(sel.trimmed(), rule)) continue;
      rule.declarations = declarations;
      rule.order = order++;
      rules.push_back(std::move(rule));
    }
  }
}

bool matchesCompound(const XmlNode& n, const Compound& c) {
  if (!c.tag.isEmpty() && c.tag != QLatin1String("*") && c.tag != n.tag) return false;
  if (!c.id.isEmpty() && n.attr(QStringLiteral("id")) != c.id) return false;
  if (!c.classes.isEmpty()) {
    const QStringList have = n.attr(QStringLiteral("class")).split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    for (const QString& cls : c.classes) {
      if (!have.contains(cls)) return false;
    }
  }
  return true;
}

bool matches(const XmlNode& n, const std::vector<Compound>& chain, int index) {
  if (!matchesCompound(n, chain[index])) return false;
  if (index == 0) return true;
  const XmlNode* p = n.parent;
  if (chain[index].link == Compound::Link::Child) return p && matches(*p, chain, index - 1);
  for (; p; p = p->parent) {
    if (matches(*p, chain, index - 1)) return true;
  }
  return false;
}

void applyTo(XmlNode& n, const std::vector<Rule>& sorted, QHash<const XmlNode*, QHash<QString, bool>>& important, int depth) {
  if (depth > kMaxXmlDepth) return;
  auto& priority = important[&n];
  for (const auto& d : n.style) priority.insert(d.first, false);  // inline declarations
  for (const Rule& rule : sorted) {
    if (!matches(n, rule.chain, int(rule.chain.size()) - 1)) continue;
    for (const Declaration& d : rule.declarations) {
      const QString camel = camelCase(d.name);
      const auto it = priority.constFind(camel);
      if (it == priority.constEnd() || (!it.value() && d.important)) {
        priority.insert(camel, d.important);
        bool replaced = false;
        for (auto& existing : n.style) {
          if (existing.first == camel) { existing.second = d.value; replaced = true; break; }
        }
        if (!replaced) n.style.push_back({camel, d.value});
      }
    }
  }
  for (auto& child : n.children) {
    if (auto* node = std::get_if<std::unique_ptr<XmlNode>>(&child)) applyTo(**node, sorted, important, depth + 1);
  }
}

void collectStyleText(const XmlNode& n, QString& out, int depth) {
  if (depth > kMaxXmlDepth) return;
  if (n.tag == QLatin1String("style")) {
    for (const auto& c : n.children) {
      if (const auto* s = std::get_if<QString>(&c)) out += *s + QLatin1Char('\n');
    }
  }
  for (const auto& c : n.children) {
    if (const auto* node = std::get_if<std::unique_ptr<XmlNode>>(&c)) collectStyleText(**node, out, depth + 1);
  }
}

}  // namespace

void applyStyleSheets(XmlNode& root) {
  QString css;
  collectStyleText(root, css, 0);
  if (css.trimmed().isEmpty()) return;
  std::vector<Rule> rules;
  int order = 0;
  collectRules(css, rules, order);
  // Highest specificity first; for equal specificity the later rule first, as
  // css.tsx applies a stable ascending sort and then reverses it.
  std::stable_sort(rules.begin(), rules.end(), [](const Rule& a, const Rule& b) {
    if (a.ids != b.ids) return a.ids > b.ids;
    if (a.classes != b.classes) return a.classes > b.classes;
    if (a.tags != b.tags) return a.tags > b.tags;
    return a.order > b.order;
  });
  QHash<const XmlNode*, QHash<QString, bool>> important;
  applyTo(root, rules, important, 0);
}

}  // namespace rnqnx::views::rnsvg::testing
