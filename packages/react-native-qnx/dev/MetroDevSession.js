'use strict';

// The injector is Metro 0.87.1's own HMRClient listener, not a bundle reload.
// Dependency injection here permits lifecycle tests without opening a socket.
function startMetroSession(
  /** @type {any} */ {
    Client,
    config,
    report,
    reload,
    schedule = setTimeout,
    cancel = clearTimeout,
  }
) {
  let disposed = false;
  /** @type {any} */
  let client;
  /** @type {any} */
  let retryTimer;
  let connectionGeneration = 0;
  let retries = 0;
  const maxRetries = 6;

  function disconnect() {
    const previous = client;
    client = undefined;
    if (previous) {
      previous.disable();
      previous.removeAllListeners();
      // Pinned Metro 0.87.1 close() waits for onclose to stop its heartbeat.
      // Stop it now and detach socket handlers so a late open cannot restart it.
      previous._stopHeartbeat();
      previous._ws.onopen = null;
      previous._ws.onmessage = null;
      previous._ws.onerror = null;
      previous._ws.onclose = null;
      previous.close();
    }
  }

  function connect() {
    if (disposed) return;
    const generation = ++connectionGeneration;
    const current = () => !disposed && generation === connectionGeneration;
    let failedUpdate = false;
    const reconnect = () => {
      if (!current()) return;
      ++connectionGeneration;
      disconnect();
      report('disconnected', 'Metro disconnected; retrying the same bundle graph');
      if (retries === maxRetries) {
        report('error', 'Metro reconnect limit reached; fix Metro then reload');
        return;
      }
      const delayMs = Math.min(250 * 2 ** retries, 4000);
      ++retries;
      retryTimer = schedule(() => {
        retryTimer = undefined;
        connect();
      }, delayMs);
    };

    try {
      client = new Client(config.hotUrl);
      const owner = client;
      const injectors = owner.listeners('update');
      if (injectors.length !== 1 || !owner._ws || typeof owner._stopHeartbeat !== 'function') {
        throw new Error('Metro HMRClient injector contract changed; re-audit the pinned runtime');
      }
      const inject = injectors[0];
      owner.removeListener('update', inject);
      owner.on('update', (/** @type {any} */ update) => {
        if (!current()) return;
        try {
          inject(update);
        } catch (error) {
          failedUpdate = true;
          report('error', String(/** @type {any} */ (error).stack || error));
        }
      });
      owner.on('update-start', () => {
        if (!current()) return;
        failedUpdate = false;
        report('updating', 'Applying Metro modules');
      });
      owner.on('update-done', () => {
        if (current() && !failedUpdate) report('ready', 'Metro update applied');
      });
      owner.on('bundle-registered', () => {
        if (!current()) return;
        retries = 0;
        report('connected', 'Metro registered the exact bundle URL');
      });
      owner.on('error', (/** @type {any} */ error) => {
        if (!current()) return;
        failedUpdate = true;
        report('error', String(error.message || error));
        if (error.type === 'GraphNotFoundError' || error.type === 'RevisionNotFoundError') {
          reload(`Metro graph lost: ${error.type}`);
        }
      });
      owner.on('connection-error', reconnect);
      owner.on('close', reconnect);
      owner.enable();
      owner.send(
        JSON.stringify({
          type: 'register-entrypoints',
          entryPoints: [config.bundleUrl],
        })
      );
    } catch (error) {
      report('error', String(/** @type {any} */ (error).stack || error));
      reconnect();
    }
  }

  connect();
  return {
    dispose() {
      if (disposed) return;
      disposed = true;
      ++connectionGeneration;
      if (retryTimer !== undefined) cancel(retryTimer);
      retryTimer = undefined;
      disconnect();
    },
  };
}

module.exports = { startMetroSession };
