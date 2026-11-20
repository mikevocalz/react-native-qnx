import Link from 'next/link';
import status from '@/generated/status.json';
import { componentRows } from '@/lib/site-data';
import { gitConfig, withBase } from '@/lib/shared';

const device = status.counts['Device-verified (Pi 400, scoped)'] ?? 0;
const host = status.counts['Host-tested'] ?? 0;
const untested = status.counts['Untested'] ?? 0;
const repoUrl = `https://github.com/${gitConfig.user}/${gitConfig.repo}`;

// The rows shown on the home page. The full list is the component matrix.
const shown = [
  'View',
  'Text',
  'Image',
  'ScrollView',
  'TextInput',
  'Pressable',
  'FlatList',
  'Modal',
  'Switch',
  'QtVideo',
  'QtAudio',
  'StatusBar',
];

const commands: { n: number; d: string; at: string; parts: [string, string][] }[] = [
  {
    n: 30,
    d: '.5s',
    at: '.25s',
    parts: [
      ['k', 'pnpm'],
      ['', ' install '],
      ['f', '--frozen-lockfile'],
    ],
  },
  {
    n: 23,
    d: '.4s',
    at: '.85s',
    parts: [
      ['k', 'pnpm'],
      ['', ' qnx init dashboard'],
    ],
  },
  {
    n: 66,
    d: '.6s',
    at: '1.35s',
    parts: [
      ['k', 'pnpm'],
      ['', ' qnx bundle-qnx '],
      ['f', '--app'],
      ['', ' dashboard '],
      ['f', '--out'],
      ['', ' build/dashboard.bundle.js'],
    ],
  },
];

function Arrow({ size }: { size: number }) {
  return (
    <svg
      viewBox={`0 0 ${size} ${size}`}
      fill="none"
      stroke="currentColor"
      strokeWidth="1.8"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      {size === 16 ? <path d="M3 8h10M9 4l4 4-4 4" /> : <path d="M4 10h12M11 5l5 5-5 5" />}
    </svg>
  );
}

const caption = (
  <>
    <b>On the board.</b> The gallery app on a Raspberry Pi 400, 1024x768. Its own heading reads
    &quot;React Native components, native Qt Quick pixels&quot;.
  </>
);

export default function HomePage() {
  const all = componentRows();
  const rows = shown
    .map((name) => all.find((c) => c.name === name))
    .filter((c): c is NonNullable<typeof c> => Boolean(c));
  const cells = [
    ...Array<string>(device).fill('v'),
    ...Array<string>(host).fill('h'),
    ...Array<string>(untested).fill('u'),
  ];

  return (
    <div className="bench">
      <main id="main">
        <section className="hero" aria-labelledby="home-title">
          <div className="wrap">
            <div className="say">
              <h1 id="home-title">React Native on&nbsp;QNX</h1>
              <p className="lede">Rendered through Fabric, Hermes, Yoga and Qt&nbsp;Quick.</p>
              <p className="what">
                An out-of-tree React Native platform for QNX. JavaScript runs on Hermes, Fabric and
                Yoga lay out the views, and a Qt Quick mounting layer draws them through QNX Screen.
              </p>
              <div className="go">
                <Link className="btn solid" href="/docs/tutorials/quick-start">
                  Quick start
                  <Arrow size={16} />
                </Link>
                <Link className="btn line" href="/docs/reference/component-matrix">
                  Component matrix
                </Link>
              </div>
              <p className="truth">
                <strong>Experimental, pre-v1.</strong> One Raspberry Pi 400 on a custom QNX 8 image
                runs it. Not on npm.
              </p>

              <figure
                className="code"
                data-wd-float
                aria-label="The first three commands, run on the host"
              >
                <header>
                  <span>On the host: Terminal</span>
                  <span>macOS or Linux</span>
                </header>
                <pre>
                  <code>
                    {commands.map((c, i) => (
                      <span className="ln" key={i}>
                        <span className="p">$ </span>
                        <span
                          className="t"
                          style={{ '--n': c.n, '--d': c.d, '--at': c.at } as React.CSSProperties}
                        >
                          {c.parts.map(([cls, text], j) =>
                            cls ? (
                              <span className={cls} key={j}>
                                {text}
                              </span>
                            ) : (
                              text
                            )
                          )}
                        </span>
                        {i === commands.length - 1 ? (
                          <span className="caret" aria-hidden="true" />
                        ) : null}
                      </span>
                    ))}
                  </code>
                </pre>
              </figure>
            </div>

            <div className="stage">
              <div className="dim" aria-hidden="true">
                1024 px
              </div>
              <figure className="monitor">
                {/* eslint-disable-next-line @next/next/no-img-element */}
                <img
                  src={withBase('/evidence/gallery-pi400.png')}
                  width={1024}
                  height={768}
                  fetchPriority="high"
                  alt="The gallery app running on the board: a dark navy window titled QNX Components, with cards for Fast Refresh, View and Text, and Image, and three teal, blue and violet bars laid out in a row."
                />
                <span
                  className="tag"
                  style={{ '--y': '6%', '--reach': '150px' } as React.CSSProperties}
                  data-wd-float
                >
                  QNX Screen<small>the window</small>
                </span>
                <span
                  className="tag"
                  style={{ '--y': '38%', '--reach': '58px' } as React.CSSProperties}
                  data-wd-float
                >
                  Qt Quick items<small>every card</small>
                </span>
                <span
                  className="tag"
                  style={{ '--y': '73%', '--reach': '64px' } as React.CSSProperties}
                  data-wd-float
                >
                  Yoga layout<small>the three bars</small>
                </span>
              </figure>

              <p className="cap">{caption}</p>
            </div>
          </div>
        </section>

        <section className="proof" id="status" aria-labelledby="status-title">
          <div className="wrap">
            <p className="cap below">{caption}</p>
            <div className="proof-head">
              <h2 id="status-title">What is proven, and what is not</h2>
              <p>
                {status.total} components, counted by where each one has actually been run. Host
                means the Qt host on a desktop. Device means the one Pi 400.
              </p>
            </div>

            <figure
              className="gauge"
              aria-label={`Component status: ${device} of ${status.total} device-verified, ${host} of ${status.total} host-tested only, ${untested} untested`}
            >
              <ol
                className="cells"
                aria-hidden="true"
                style={{ gridTemplateColumns: `repeat(${cells.length}, 1fr)` }}
              >
                {cells.map((c, i) => (
                  <li className={c} key={i} />
                ))}
              </ol>
              <ul
                className="read"
                style={
                  { '--rq-split': `${device}fr ${host}fr ${untested}fr` } as React.CSSProperties
                }
              >
                <li>
                  <i className="sw v" aria-hidden="true" />
                  <b>
                    {device}
                    <i>of {status.total}</i>
                  </b>
                  <span>
                    device-verified <small>Pi 400, scoped</small>
                  </span>
                </li>
                <li>
                  <i className="sw h" aria-hidden="true" />
                  <b>
                    {host}
                    <i>of {status.total}</i>
                  </b>
                  <span>
                    host-tested only <small>not yet run on a board</small>
                  </span>
                </li>
                <li>
                  <i className="sw u" aria-hidden="true" />
                  <b>
                    {untested}
                    <i>of {status.total}</i>
                  </b>
                  <span>untested</span>
                </li>
              </ul>
            </figure>

            <div className="gates">
              <div className="rings" aria-hidden="true">
                {Array.from({ length: status.gatesTotal }, (_, i) => (
                  <i
                    className={i < status.gatesTotal - status.gatesPending ? 'done' : undefined}
                    key={i}
                  />
                ))}
              </div>
              <p>
                <b>
                  {status.gatesPending} of {status.gatesTotal}
                </b>{' '}
                v1 release gates pending.{' '}
                <Link className="inline" href="/docs/reference/status">
                  Read the limits
                </Link>
              </p>
              <p className="not">No stock Pi 4 or Pi 5 result.</p>
            </div>
            <p className="pins">React Native 0.88.1 / QNX SDP 8.0.5 / Expo SDK 58 / MIT</p>

            <div className="lower">
              <div id="components">
                <h3>Component by component</h3>
                <table>
                  <thead>
                    <tr>
                      <th scope="col">Component</th>
                      <th scope="col">Host</th>
                      <th scope="col">Pi 400</th>
                    </tr>
                  </thead>
                  <tbody>
                    {rows.map((c) => (
                      <tr key={c.name}>
                        <th scope="row">{c.name}</th>
                        <td>{c.host}</td>
                        <td className={c.pi400 === 'scoped' ? 'ok' : undefined}>{c.pi400}</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
                <p className="note">
                  Scoped means only the linked scenario ran on the one board. Unverified means no
                  device receipt.
                </p>
                <Link className="more" href="/docs/reference/component-matrix">
                  Component matrix, all {status.total}
                </Link>
              </div>
              <div>
                <h3>Start from where you are</h3>
                <ul className="paths">
                  <li>
                    <Link href="/docs/tutorials/quick-start">
                      <b>I have a Mac and an Expo app</b>
                      <span>Create and bundle an app on the host</span>
                      <Arrow size={20} />
                    </Link>
                  </li>
                  <li>
                    <Link href="/docs/tutorials/first-device-run">
                      <b>I have a QNX SDP and a board</b>
                      <span>See what a Pi 400 run needs</span>
                      <Arrow size={20} />
                    </Link>
                  </li>
                  <li>
                    <Link href="/docs/explanation/threading">
                      <b>I write native modules</b>
                      <span>Learn the threading rules first</span>
                      <Arrow size={20} />
                    </Link>
                  </li>
                </ul>
                <figure className="shot2">
                  {/* eslint-disable-next-line @next/next/no-img-element */}
                  <img
                    src={withBase('/evidence/svg-pi400.png')}
                    width={1024}
                    height={768}
                    loading="lazy"
                    alt="The gallery app, Advanced tab: SVG gradients, clip paths, masks and text drawn on the board."
                  />
                  <figcaption>Also from the Pi 400, 1024x768.</figcaption>
                </figure>
              </div>
            </div>
          </div>
        </section>
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
