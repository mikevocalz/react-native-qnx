# Experimental single-window API contract

The native host owns one window. `getCurrentWindow()` returns an independent
handle; it never creates a window or changes the configured dimensions. Cheap
`snapshot` reads are cached. Mutations cross once to the Qt GUI owner and return
Promises. Focus requests are best effort: the snapshot reports actual focus.
Unsupported position, resize or minimize operations reject. Sizes and positions
are logical pixels. Handle disposal stops its listeners/guards; another handle
keeps working. Window/host shutdown rejects pending commands and stops callbacks.

```ts
const window = getCurrentWindow();
const changes = window.addSnapshotListener((snapshot) => renderInsets(snapshot.insets));
await window.setTitle('Editor');
try {
  await window.setSize(900, 650);
} catch (error) {
  showError(error); // unsupported target or invalid dimensions
}
changes.remove();
window.dispose();
```

A close guard vetoes both native chrome and API close requests without calling
JavaScript synchronously. `cancelCloseRequest(requestId)` clears the matching pending dialog state
without releasing any guard. A blocked JS thread therefore cannot accidentally
approve closure. Removing a guard does not close the window; confirmation is a
separate explicit request. All active guards must be removed to allow closure.

```ts
const window = getCurrentWindow();
const guard = window.addCloseGuard();
const changes = window.addSnapshotListener((snapshot) => {
  if (snapshot.closeRequested) showSaveDialog(snapshot.closeRequestId);
  // showSaveDialog deduplicates the same request ID on unrelated window updates.
});
// Cancel: await window.cancelCloseRequest(dialogRequestId); retain the guard.
// A stale request ID resolves false and cannot cancel a later close request.
// Confirm after saving:
guard.remove();
await window.close();
changes.remove();
window.dispose();
```

Registration uses the installed Nitro v1 alpha136 HybridClass/resource model.
The host root is runtime-specific; no process-global current-window selector or
arbitrary privileged command dispatcher is exposed. The constrained repository
binding generator is explicitly separate from upstream Nitrogen's target support.
Multi-window creation is outside this API; independent handles share the same
app-owned window. No hardware acceptance follows from host tests.
