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

const DefaultImg = defaultMdxComponents.img as (props: ComponentProps<'img'>) => React.ReactNode;

// A site-absolute string src (/evidence/x.png) needs the deploy base path. Imported images
// (objects) and external URLs pass through untouched.
export function Img({ src, ...props }: ComponentProps<'img'>) {
  return <DefaultImg {...props} src={typeof src === 'string' ? withBase(src) : src} />;
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
