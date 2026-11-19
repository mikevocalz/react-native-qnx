import Link from 'next/link';
import status from '@/generated/status.json';

const device = status.counts['Device-verified (Pi 400, scoped)'] ?? 0;
const host = status.counts['Host-tested'] ?? 0;
const untested = status.counts['Untested'] ?? 0;

export default function HomePage() {
  return (
    <main className="mx-auto w-full max-w-5xl flex-1 px-4 py-16 sm:px-6">
      <p className="text-sm font-medium text-fd-muted-foreground">Experimental, pre-v1</p>
      <h1 className="mt-3 max-w-3xl text-4xl font-semibold tracking-tight sm:text-5xl">
        React Native on QNX, with Fabric, Hermes and Qt Quick
      </h1>
      <p className="mt-5 max-w-2xl text-lg text-fd-muted-foreground">
        Create an app, bundle it for QNX, and see exactly which components have run on a board.
        Every status below links to the commit behind it.
      </p>

      <div className="mt-8 flex flex-wrap items-center gap-3">
        <Link
          href="/docs/tutorials/quick-start"
          className="rounded-md bg-fd-primary px-4 py-2 text-sm font-medium text-fd-primary-foreground"
        >
          Quick start
        </Link>
        <Link
          href="/docs/reference/component-matrix"
          className="rounded-md border border-fd-border px-4 py-2 text-sm font-medium"
        >
          Component matrix
        </Link>
      </div>

      <figure className="mt-10 max-w-2xl">
        <figcaption className="mb-2 text-sm text-fd-muted-foreground">
          The first three steps of the quick start
        </figcaption>
        <pre className="overflow-x-auto rounded-md border border-fd-border bg-fd-card p-4 text-sm">
          <code>{`pnpm install --frozen-lockfile
pnpm qnx init dashboard
pnpm qnx bundle-qnx --app dashboard --out build/dashboard.bundle.js`}</code>
        </pre>
      </figure>

      <section aria-labelledby="status-heading" className="mt-16">
        <h2 id="status-heading" className="text-2xl font-semibold tracking-tight">
          Where things stand
        </h2>
        <dl className="mt-6 grid gap-4 sm:grid-cols-3">
          <div className="rounded-md border border-fd-border p-4">
            <dt className="text-sm text-fd-muted-foreground">Device-verified, Pi 400, scoped</dt>
            <dd className="mt-1 text-3xl font-semibold">
              {device} <span className="text-base font-normal">of {status.total}</span>
            </dd>
          </div>
          <div className="rounded-md border border-fd-border p-4">
            <dt className="text-sm text-fd-muted-foreground">Host-tested only</dt>
            <dd className="mt-1 text-3xl font-semibold">
              {host} <span className="text-base font-normal">of {status.total}</span>
            </dd>
          </div>
          <div className="rounded-md border border-fd-border p-4">
            <dt className="text-sm text-fd-muted-foreground">Untested</dt>
            <dd className="mt-1 text-3xl font-semibold">
              {untested} <span className="text-base font-normal">of {status.total}</span>
            </dd>
          </div>
        </dl>
        <p className="mt-4 max-w-2xl text-sm text-fd-muted-foreground">
          {status.gatesPending} of {status.gatesTotal} v1 release gates are pending. All device
          results come from one Raspberry Pi 400 with a custom image. No stock Pi 4 has run.{' '}
          <Link href="/docs/reference/status" className="underline">
            Read the limits.
          </Link>
        </p>
      </section>

      <section aria-labelledby="paths-heading" className="mt-16">
        <h2 id="paths-heading" className="text-2xl font-semibold tracking-tight">
          Start where you are
        </h2>
        <ul className="mt-6 grid gap-4 sm:grid-cols-3">
          <li className="rounded-md border border-fd-border p-4">
            <Link href="/docs/tutorials/quick-start" className="font-medium underline">
              I have a Mac and an Expo app
            </Link>
            <p className="mt-2 text-sm text-fd-muted-foreground">
              Create and bundle an app on the host.
            </p>
          </li>
          <li className="rounded-md border border-fd-border p-4">
            <Link href="/docs/tutorials/first-device-run" className="font-medium underline">
              I have a QNX SDP and a board
            </Link>
            <p className="mt-2 text-sm text-fd-muted-foreground">See what a Pi 400 run needs.</p>
          </li>
          <li className="rounded-md border border-fd-border p-4">
            <Link href="/docs/explanation/threading" className="font-medium underline">
              I write native modules
            </Link>
            <p className="mt-2 text-sm text-fd-muted-foreground">
              Learn the threading rules first.
            </p>
          </li>
        </ul>
      </section>
    </main>
  );
}
