# Layout references

Mobbin was used for layout references, searched on 2026-11-19. No image was copied into the repository. Each link opens the screen on Mobbin.

## Landing page

| Reference                                                                                                  | Pattern borrowed                                                                               |
| ---------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------- |
| [Vercel, developer framework hero](https://mobbin.com/sites/sections/23fc99c3-5a39-45ae-9b49-7015e3193042) | One headline, one install command, one "read docs" button. Used for the home page hero.        |
| [Mural, "for developers"](https://mobbin.com/sites/sections/d98fd0c4-8ff4-4ef8-bf82-c2597b1edb38)          | A plain statement of what it is above a single docs call to action.                            |
| [Anchor, "made for developers"](https://mobbin.com/sites/sections/d99917c8-d6a4-42f2-92ce-00e32442c35c)    | A code sample next to the headline. Used as the reason to show real commands on the home page. |

## Quick start

| Reference                                                                                         | Pattern borrowed                                             |
| ------------------------------------------------------------------------------------------------- | ------------------------------------------------------------ |
| [LangChain, create an app](https://mobbin.com/screens/568fff70-41fd-4660-a3b6-eb843e6e14d9)       | Numbered steps, one command per step, short prose.           |
| [Mintlify, introduction](https://mobbin.com/screens/d4ce0aef-c205-432b-8f31-b4267315dc35)         | Left page tree, centered column, "next" cards at the end.    |
| [OpenAI Platform, docs overview](https://mobbin.com/screens/1b39571a-c929-4ba6-934c-9786fc50681d) | Grouped left navigation and task cards on the overview page. |

## Status matrix

| Reference                                                                                             | Pattern borrowed                                                                                       |
| ----------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------ |
| [Figma, compare all features](https://mobbin.com/sites/sections/4bd2639f-6606-4ccc-be23-b8bfe524e453) | Dense rows, a status column, a rule under the header.                                                  |
| [Stripe, compare plans](https://mobbin.com/sites/sections/4fa7688a-e161-408b-831c-a5827cd77558)       | A dash for "not available". The matrix uses words instead, since color or a glyph alone is not enough. |
| [Vanta, capabilities](https://mobbin.com/sites/sections/6197feb2-3021-411c-babd-6c55ccd12900)         | Group headings above the rows.                                                                         |

## API reference

| Reference                                                                                    | Pattern borrowed                                                              |
| -------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------- |
| [AirOps, REST API](https://mobbin.com/screens/a160611c-6fec-46bc-88c1-4aff9898cd36)          | Parameters as a name, type and description list.                              |
| [GitBook, API reference](https://mobbin.com/screens/5f15798e-7b7e-43f2-9a66-740564ad9a26)    | Request on the left, example on the right. Not used: the CLI has no HTTP API. |
| [Base44, API documentation](https://mobbin.com/screens/67828c1b-90e1-4e8d-89a1-9e6f6cec99b4) | Code block with a copy button, then a schema table.                           |

Mobbin previews are low resolution and cover products unrelated to QNX. They informed layout only. Visual design choices (neutral palette, system fonts, no gradients, reduced motion support) come from the design review checklist, not from copying these screens.

## Changelog and component pages (docs parity pass, 2026-11-19)

Searched Mobbin for a changelog and for a component reference page. The component search returned no documentation screens (Productboard, Braintrust, Subframe and Stripe dashboard screens), so no layout was taken from it. The changelog search returned one usable pattern.

| Reference                                                                                          | Pattern borrowed                                                                                                                                  |
| -------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------- |
| [incident.io, "What's new" panel](https://mobbin.com/screens/92fe8695-3d16-4e2b-8b9a-23cf01fc5a7d) | A date and a one-line title per entry, newest first. The changelog uses a table per month with the same two fields plus the PR number and commit. |
| [Stripe, API versions list](https://mobbin.com/screens/0d1defeb-2907-4c9e-be0d-664f5fef0f89)       | A small "Latest" or "Default" badge beside a version. Not used: there are no versions to badge.                                                   |

Weak matches, not used: [Productboard releases](https://mobbin.com/screens/5d055c4b-a5cf-4113-9246-bb879a4004a6), [Upwork weekly summary](https://mobbin.com/screens/270263a9-c419-4eeb-9e93-848e7f0bfbe9), [Plane release changelog tab](https://mobbin.com/screens/c3220fd6-b32d-4ae9-8764-64617ec006c2).

## Visual direction (redesign, 2026-11-19)

The look named "Bench" replaces the neutral palette and system fonts described above. It was chosen from three rendered explorations. Three live sites set the structure; no logo, wordmark, illustration, text or code was copied from any of them.

| Reference                                                  | Pattern borrowed                                                                                                                                                                                 | Where it is used                               |
| ---------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ---------------------------------------------- |
| [VisionCamera docs](https://visioncamera.margelo.com/docs) | One flat cool grey ground with hairlines, a heavy slanted wordmark, top bar with a centred search, grouped sidebar with a tinted active row, "Copy Markdown" and "Open", a right "On this page". | Every page under `/docs` (the Notebook layout) |
| [reactnative.dev](https://reactnative.dev/)                | Name, one line and two buttons first, then the code beside the thing it produces.                                                                                                                | `/` (home)                                     |
| [Reactylon releases](https://www.reactylon.com/releases)   | One reading column with no sidebar, a ruled heading per group, prose and bullets under it.                                                                                                       | `/releases`                                    |

Decisions:

- Type: [Archivo](https://fonts.google.com/specimen/Archivo) with its width axis (expanded heavy italic for the wordmark, the home headline, step numbers and status figures; normal width for text) and [Azeret Mono](https://fonts.google.com/specimen/Azeret+Mono) for code, figures and measured labels. Both load through `next/font/google`, which downloads them at build time and serves them from the site.
- Colour: one teal, taken from the gallery app that runs on the board (`#7BE8DC` on the device, darkened to `#0A6A62` for text on the light ground). Tokens are the `--rq-*` variables in `src/app/global.css`; the Fumadocs colours read from them, in light and dark.
- Imagery: only real captures from the Pi 400, copied from `docs/evidence/pr149-pi400-run2-2026-11-19/` into `public/evidence/`.
- Code tabs: the repository pins pnpm, so there are no npm, yarn or bun tabs.
