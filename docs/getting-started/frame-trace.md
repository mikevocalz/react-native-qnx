# Qt frame submission diagnostics

The existing default-OFF `RNQNX_ENABLE_FRAME_TRACE` option can observe a
`QQuickWindow` through `QtFrameTrace`. Attach it on the window owner thread and
keep the trace alive for the capture. Its callbacks record into the existing
bounded native recorder; they retain neither Hermes nor a Qt item or observer.
Disconnect stops future signal delivery. A callback already executing can
finish recording into its original trace session, never a replacement runtime.

`qt.frame.queued` means Qt emitted `frameSwapped`: a frame was queued for
presenting. It is not a compositor or physical-display timestamp. Qt documents
that signal on the scene-graph render thread, so the observer uses a direct
connection and does not hop through the GUI event queue. A null trace does not
attach an observer. Window destruction disconnects it; trace destruction makes
later callbacks inert. Surface/generation zero denotes a window-wide event,
not an identified Fabric generation.

The recorder uses session-relative `std::chrono::steady_clock` nanoseconds.
Timestamps include recorder-lock contention and are not aligned to QNX Screen
or display clocks. Overflow is reported; a frozen session admits no new records.
Export after the workload stops, outside rendering callbacks. These samples do
not include process creation, loader time, RSS or calibrated presentation, so
they cannot complete the v1 performance gate or replace its capture contract.

The host `rnqnx_qt_frame_trace_test` checks signal delivery, disconnection,
bounded storage, freeze, thread affinity and independent lifetimes. It invokes
the Qt signal directly and does not render a workload. It is not registered as
a QNX cross-build execution test.

Sources: [Qt 6.12 QQuickWindow::frameSwapped](https://doc.qt.io/qt-6/qquickwindow.html#frameSwapped),
[existing performance plan](../platform-modernization/03-frame-pacing.md).
QNX 8 [window metrics](https://qnx.com/developers/docs/8.0/com.qnx.doc.screen/topic/group__screen__windows_1Screen_Window_Metric_Count_Types.html)
and [timestamp properties](https://qnx.com/developers/docs/8.0/com.qnx.doc.screen/topic/screen_8h_1Screen_Property_Types.html)
must not be relabeled as monotonic presentation samples: the documented frame
timestamp is application-set and Screen calculates timestamps using realtime.

Implementation follows the pinned native skills recorded in
[SKILLS.md](../platform-modernization/SKILLS.md): API design keeps the boundary
explicit; Nitro guidance avoids an unnecessary public binding; C++ guidance
keeps ownership weak and callbacks free of observer state; React Native
performance guidance keeps submission and presentation separate. Human
measurement reviewer and release DRI remain unassigned. Disable frame tracing
to remove the observer; no boot, renderer or scheduling-policy change is needed.

The playground and consumer executables attach the observer to their window when built with `RNQNX_ENABLE_FRAME_TRACE=ON` and run with `--trace-json`, so `qt.frame.queued` events land in the same trace file as the runtime stages.
