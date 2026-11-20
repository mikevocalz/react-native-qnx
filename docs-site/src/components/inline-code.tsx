import type { ReactNode } from 'react';

// Renders `code` spans in a plain string. Used for changelog text read from Markdown.
export function InlineCode({ text }: { text: string }): ReactNode {
  return text
    .split(/(`[^`]+`)/g)
    .map((part, i) =>
      part.startsWith('`') && part.endsWith('`') ? <code key={i}>{part.slice(1, -1)}</code> : part
    );
}
