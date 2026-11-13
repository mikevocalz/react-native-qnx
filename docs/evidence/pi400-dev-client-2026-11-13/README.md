# Pi 400 development client, 2026-11-13

Board: Raspberry Pi 400, QNX SDP 8.0.5 image, public-source Qt 6.12.0 (-O2
runtime), `rnqnx-winmgr`. Computer: Mac on the USB Ethernet link
(192.168.2.1). App: `apps/component-gallery` built with
`RNQNX_ENABLE_DEV_SUPPORT=ON`, launched by `rnqnx project dev`.

| File                            | What it shows                                                                                  |
| ------------------------------- | ---------------------------------------------------------------------------------------------- |
| `1-counter-3.jpg`               | Counter in `FastRefreshCard.js` raised to 3 through the developer menu item the card registers |
| `2-fast-refresh-state-kept.jpg` | After editing `HEADLINE` and `ACCENT` on the Mac: new text and colour, counter still 3         |
| `3-logbox-syntax-error.jpg`     | Syntax error saved on the Mac: LogBox surface over the app; fixing it hid LogBox               |
| `4-logbox-render-error.jpg`     | A throw during render: LogBox with component and call stacks                                   |
| `5-dev-menu-from-metro.jpg`     | `d` pressed in the Metro terminal opens the developer menu on the board                        |
| `6-metro-unreachable.jpg`       | Reload with Metro stopped: native error screen; the app loaded again when Metro came back      |
| `device-log-excerpt.txt`        | `RNQNX_DEV_*` lines from `/tmp/rnqnx-showcase-dev.log`                                         |
| `metro-terminal.txt`            | The `rnqnx project dev` terminal, including `r` and `d`                                        |

Input was injected through Screen (`rnqnx-inject key`): pointer injection did not
reach the right coordinates under the current window manager, so the counter
was driven from the keyboard-navigable developer menu.
