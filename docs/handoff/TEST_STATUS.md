# TEST_STATUS.md

## Live test matrix

| Test suite | Host/Target | Config | Last result | Date executed | Known failures | Associated issue |
| --- | --- | --- | --- | --- | --- | --- |
| doctor.sh | macOS arm64 (host) | N/A | Failed | 2026-07-19 | QNX SDP 8.0.5 not installed; QNX_HOST/QNX_TARGET unset; qcc/q++/Screen headers missing; ninja missing | – |
| configure-qnx.sh aarch64 Debug | macOS arm64 (host) | Debug | Failed | 2026-07-19 | QNX_HOST/QNX_TARGET unset; toolchain fatal error | – |

## Notes

- The doctor correctly detected that the current macOS arm64 host is not a supported QNX build host.
- The configure script correctly failed at the toolchain file with an exact message identifying the missing `QNX_HOST` environment variable.
- No QNX target hardware has been exercised.
