'use strict';

// The fixture data never reads device properties, time, network, or user data.
const SCENARIOS = Object.freeze([
  {
    id: 'GALLERY-001',
    title: 'Public React Native component gallery',
    page: 'gallery',
    status: 'not-run',
    expectation:
      'Observe the bundled image, press, edit, toggle, open and dismiss the modal; retain state between tabs.',
  },
  {
    id: 'SECTIONS-001',
    title: 'Grouped virtualized list',
    page: 'sections',
    status: 'not-run',
    expectation:
      'Scroll through grouped rows, select a row and retain selection after changing tabs.',
  },
  {
    id: 'LIST-001',
    title: '1,000-row document list',
    page: 'list',
    status: 'not-run',
    expectation: 'Scroll, select row 0500 and return without stale selection.',
  },
  {
    id: 'INPUT-001',
    title: 'Controlled input and buttons',
    page: 'controls',
    status: 'not-run',
    expectation: 'Edit text, switch state, press twice and retain the visible values.',
  },
  {
    id: 'LAYOUT-001',
    title: 'Single/two-column layout',
    page: 'adaptive',
    status: 'not-run',
    expectation: 'Resize between narrow and wide layouts without losing the selected row.',
  },
  {
    id: 'DRAWER-001',
    title: 'Interruptible gesture drawer',
    page: 'blocked',
    status: 'blocked',
    expectation: 'Requires proven RNGH/Worklets interaction on the physical target.',
  },
  {
    id: 'IME-001',
    title: 'Multilingual IME composition',
    page: 'blocked',
    status: 'blocked',
    expectation: 'Requires real software keyboard/IME provider and named font coverage.',
  },
  {
    id: 'NATIVE-001',
    title: 'Non-premium Nitro document fixture',
    page: 'blocked',
    status: 'blocked',
    expectation: 'Requires a generated, ABI-aligned QNX Nitro v1 document binding.',
  },
  {
    id: 'SERVICE-001',
    title: 'Permissioned system subscription',
    page: 'blocked',
    status: 'blocked',
    expectation: 'Requires native capability, permission-denied and disposal tests.',
  },
  {
    id: 'LIFE-001',
    title: 'Background and state restoration',
    page: 'blocked',
    status: 'blocked',
    expectation: 'Requires a tested QNX lifecycle adapter and restart observations.',
  },
]);

function createDocumentRows(count = 1000) {
  if (!Number.isSafeInteger(count) || count < 1 || count > 1000) {
    throw new RangeError('Fixture row count must be an integer from 1 to 1000.');
  }
  return Array.from({ length: count }, (_, index) => {
    const number = String(index + 1).padStart(4, '0');
    return {
      id: 'document-' + number,
      title: 'Document ' + number,
      subtitle: 'Synthetic conformance item ' + number,
    };
  });
}

module.exports = { SCENARIOS, createDocumentRows };
