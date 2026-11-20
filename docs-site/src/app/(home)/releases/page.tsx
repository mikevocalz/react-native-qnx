import type { Metadata } from 'next';
import Link from 'next/link';
import status from '@/generated/status.json';
import { InlineCode } from '@/components/inline-code';
import { mergedPulls, unreleasedNotes } from '@/lib/site-data';
import { gitConfig } from '@/lib/shared';

export const metadata: Metadata = {
  title: 'Releases',
  description:
    'React Native QNX has no tagged releases yet. This page lists the unreleased notes and every merged pull request, newest first.',
};

const repoUrl = `https://github.com/${gitConfig.user}/${gitConfig.repo}`;

export default function ReleasesPage() {
  const notes = unreleasedNotes();
  const months = mergedPulls();
  const total = months.reduce((n, m) => n + m.pulls.length, 0);
  if (total !== status.pulls.total)
    throw new Error(`releases: parsed ${total} pull requests, status.json says ${status.pulls.total}`);

  return (
    <div className="bench">
      <main id="main" className="rel">
        <h1>Releases</h1>
        <p>
          <strong>There are no tagged releases.</strong> This site describes <code>main</code>.
          React Native QNX is experimental, pre-v1, and it is not on npm. {status.gatesPending} of{' '}
          {status.gatesTotal} <Link href="/docs/reference/release-gates">v1 release gates</Link> are
          pending.
        </p>
        <p>
          The notes below come from{' '}
          <a href={`${repoUrl}/blob/${gitConfig.branch}/CHANGELOG.md`}>CHANGELOG.md</a>. The pull
          request list is generated from git history; the same list, with how the project is
          versioned, is in the <Link href="/docs/reference/changelog">changelog reference</Link>.
        </p>

        {notes.length ? (
          <>
            <h2 id="unreleased">
              Unreleased <small>main, no tag</small>
            </h2>
            {notes.map((group) => (
              <section key={group.heading} aria-label={group.heading}>
                <h3>{group.heading}</h3>
                <ul className="notes">
                  {group.items.map((item, i) => (
                    <li key={i}>
                      <InlineCode text={item} />
                    </li>
                  ))}
                </ul>
              </section>
            ))}
          </>
        ) : null}

        <h2 id="merged">
          Merged pull requests <small>{total} merged</small>
        </h2>
        <p>
          Every merged pull request in git history through <code>{status.pulls.through}</code> is
          listed. Each number comes from the merge or squash subject git recorded.
          {status.pulls.titleOmitted
            ? ` ${status.pulls.titleOmitted} titles name a separate target and are shown as "title omitted"; the pull request number links to the full title.`
            : ''}
        </p>
        {months.map((m) => (
          <section key={m.month} aria-label={m.month}>
            <h3>{m.month}</h3>
            <ol className="prs">
              {m.pulls.map((p) => (
                <li key={p.number}>
                  <a className="pr" href={p.url}>
                    #{p.number}
                  </a>
                  <span className="ti">
                    <InlineCode text={p.title} />
                  </span>
                  <code className="sha">{p.sha}</code>
                  <time dateTime={p.date}>{p.date}</time>
                </li>
              ))}
            </ol>
          </section>
        ))}
      </main>

      <footer className="foot">
        <div className="wrap">
          <span>React Native QNX. MIT licence.</span>
          <a href={repoUrl}>
            github.com/{gitConfig.user}/{gitConfig.repo}
          </a>
        </div>
      </footer>
    </div>
  );
}
