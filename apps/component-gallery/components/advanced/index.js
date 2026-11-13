// The Advanced tab's cards. Each card is one file in this directory that
// default-exports { id, title, detail, order, Component }. Metro's
// require.context finds every *.card.js file here, so adding a card never
// edits this file and parallel pull requests do not conflict. See
// apps/component-gallery/README.md ("Adding an Advanced card").
const context = require.context('./', false, /\.card\.js$/);

function validate(file, card) {
  const where = `components/advanced/${file.replace('./', '')}`;
  if (!card || typeof card !== 'object') throw new Error(`${where} must default-export a card`);
  for (const key of ['id', 'title', 'detail']) {
    if (typeof card[key] !== 'string' || !card[key])
      throw new Error(`${where}: ${key} must be a string`);
  }
  if (typeof card.Component !== 'function')
    throw new Error(`${where}: Component must be a component`);
  return card;
}

/** Cards sorted by `order` (default 100), then by `id`. */
const ADVANCED_CARDS = context
  .keys()
  .map((file) => validate(file, context(file).default))
  .sort((a, b) => (a.order ?? 100) - (b.order ?? 100) || a.id.localeCompare(b.id));

const ids = new Set();
for (const card of ADVANCED_CARDS) {
  if (ids.has(card.id)) throw new Error(`Two Advanced cards use the id "${card.id}"`);
  ids.add(card.id);
}

module.exports = { ADVANCED_CARDS };
