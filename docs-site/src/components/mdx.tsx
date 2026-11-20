import defaultMdxComponents from 'fumadocs-ui/mdx';
import type { MDXComponents } from 'mdx/types';
import type { ComponentProps } from 'react';
import { basePath } from '@/lib/shared';

// Plain <img> is not prefixed by Next's basePath. Frozen release builds live
// under /v/<tag>, so site-absolute image paths need the prefix added here.
function Img(props: ComponentProps<'img'>) {
  const src =
    typeof props.src === 'string' && props.src.startsWith('/') ? basePath + props.src : props.src;
  // eslint-disable-next-line @next/next/no-img-element, jsx-a11y/alt-text
  return <img {...props} src={src} />;
}

export function getMDXComponents(components?: MDXComponents) {
  return {
    ...defaultMdxComponents,
    img: Img,
    Img,
    ...components,
  } satisfies MDXComponents;
}

export const useMDXComponents = getMDXComponents;

declare global {
  type MDXProvidedComponents = ReturnType<typeof getMDXComponents>;
}
