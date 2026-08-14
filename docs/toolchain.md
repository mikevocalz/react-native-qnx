# QNX Toolchain Evidence

## Gate status

G0-A is **blocked**, not passed. The current macOS arm64 workspace has no QNX
SDP environment, `qcc`, `q++`, sysroot, configured QSTI Pi, or target SSH user.
No C++ translation unit has been compiled with qcc and no QNX binary has run.

## Publicly verified configuration

The current QNX Everywhere Porting Guide's CMake example specifies:

- `CMAKE_SYSTEM_NAME` = `QNX`;
- `CMAKE_SYSTEM_PROCESSOR` = `aarch64`;
- `q++` as the C++ compiler;
- `gcc_ntoaarch64le` as `CMAKE_CXX_COMPILER_TARGET`;
- the installed `QNX_TARGET` as `CMAKE_SYSROOT`.

The QNX SDP 8 Utilities Reference also lists `gcc_ntoaarch64le` and directs
developers to run `qcc -V` for the installed compiler/version/target list.
Those sources authorize the prepared
`vnext/cmake/qnx-aarch64le.toolchain.cmake`; they do not substitute for the
required installed output.

The QNX Everywhere June 19, 2026 public release notes list Raspberry Pi 4 QSTI
package `com.qnx.qnx800.quickstart.rpi4` at package version
`0.4.0.00297T202606061645L` / build 297 and say that QSTI/CTI release supports
QNX SDP 8.0.4. That is registry documentation, not the identity of a target in
this project; the installed package/build must still be captured from the Pi.

Sources:

- [QNX Everywhere Porting Guide — CMake](https://www.qnx.com/developers/docs/qnxeverywhere/com.qnx.doc.qpg/topic/port_CMake.html)
- [QNX SDP 8 Utilities Reference — q++, qcc](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.utilities/topic/q/qcc.html)
- [QNX Everywhere — installing the Raspberry Pi QSTI](https://www.qnx.com/developers/docs/qnxeverywhere/com.qnx.doc.target_images/topic/qsti/install.html)
- [QNX Everywhere release notes](https://www.qnx.com/developers/docs/qnxeverywhere/rel_notes.html)

## Required installed evidence

Run the following only on the licensed x86-64 Linux QNX development host with
the SDP environment sourced and explicit target SSH variables set:

```bash
export QNX_TARGET_IP=<pi-host-or-address>
export QNX_TARGET_USER=<verified-qsti-user>
# Optional when the key is not already available through SSH configuration:
export QNX_SSH_KEY=<private-key-path>

./scripts/validate-g0-a.sh
```

The script captures local evidence under ignored `artifacts/g0/`, requires the
installed `qcc -V` list to contain `gcc_ntoaarch64le`, cross-builds the probe,
copies that exact binary to the Pi, verifies its deterministic output, and
enforces the specification's under-30-second deploy/run threshold.

After the run, review the evidence for account, license, host, and path data.
Commit only a sanitized transcript here containing:

1. SDP and QSTI package/build identities;
2. the complete safe `qcc -V` and `q++ -V` compiler/target lines;
3. the configure and build result for preset `qnx-pi4`;
4. `RNQNX_G0_CXX20_OK ranges=12 designated=21 jthread=34` from the Pi;
5. measured deploy-and-run seconds, target model, and QNX OS identity.

Do not replace this blocked status with anticipated output.
