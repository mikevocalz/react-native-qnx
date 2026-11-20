import defaultMdxComponents from 'fumadocs-ui/mdx';
import type { MDXComponents } from 'mdx/types';
import type { ComponentProps } from 'react';
import { withBase } from '@/lib/shared';

const DefaultH2 = defaultMdxComponents.h2 as (props: ComponentProps<'h2'>) => React.ReactNode;

// "## 1. Clone and install" renders its step number in the display face.
function StepHeading({ children, ...props }: ComponentProps<'h2'>) {
  const parts = Array.isArray(children) ? [...children] : [children];
  const first = parts[0];
  const match = typeof first === 'string' ? first.match(/^(\d+)\.\s+(.*)$/s) : null;
  if (!match) return <DefaultH2 {...props}>{children}</DefaultH2>;
  return (
    <DefaultH2 {...props}>
      <span className="rq-step">{match[1]}</span>
      {match[2]}
      {parts.slice(1)}
    </DefaultH2>
  );
}

// A raw <img> is not prefixed by Next's basePath. Frozen release builds live
// under /<base>/v/<tag>, so site-absolute image paths get the prefix here.
function Img(props: ComponentProps<'img'>) {
  const src = typeof props.src === 'string' ? withBase(props.src) : props.src;
  // eslint-disable-next-line @next/next/no-img-element, jsx-a11y/alt-text
  return <img {...props} src={src} />;
}

export function getMDXComponents(components?: MDXComponents) {
  return {
    ...defaultMdxComponents,
    h2: StepHeading,
    img: Img,
    Img,
    ...components,
  } satisfies MDXComponents;
}

export const useMDXComponents = getMDXComponents;

declare global {
  type MDXProvidedComponents = ReturnType<typeof getMDXComponents>;
}
