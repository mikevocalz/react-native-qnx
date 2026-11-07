# Finite systems probe implementation receipt

Source base: `24019f39a409cd0540c31f4e017b7003201bdca4`.
Implementation: `61258b13bbd39dea9c5f9985d5922637e4018a42` on
`codex/qnx-systems-probes`. The lockfile is unchanged; SHA-256:
`e9602379b5849218cef9718da6db4430e36b51382fd787dcfbecb8269c0aaa52`.
This implements the first portable native slice of Plan 14. Other Plan 14
acceptance criteria remain open.

The diagnostic target has no RN/Qt/Hermes/Nitro linkage. Its root and standalone
option defaults OFF. Supported systems are QNX, Linux and Darwin; unsupported
systems fail configuration when enabled. No runtime dependency, public native
API, package, app window, graphics or boot configuration changed. No archived
source or premium code was copied.

The systems implementation agent is the C++ systems/build DRI. The separate
`renderer_build_audit` agent reviewed all source files and the root CMake hook
and reported no blocking finding. Human release assignments remain unassigned.
The review requires an external deadline because joining workers is not itself
a deadline mechanism.

| Gate                               | Result                  | Command and evidence                                                                                                                                                                                                                                                                                                                                                                                                           |
| ---------------------------------- | ----------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| macOS host                         | Passed                  | `cmake -S tests/native/qnx-systems -B build/systems-host -DRNQNX_BUILD_QNX_SYSTEMS_PROBES=ON -DCMAKE_BUILD_TYPE=Debug`; `cmake --build build/systems-host --parallel 2`; `ctest --test-dir build/systems-host --output-on-failure --repeat until-fail:10`. AppleClang 21.0.0.21000334; ASan/UBSan; 3 tests, 10 executions each, all passed.                                                                                    |
| Linux host                         | Passed                  | Same standalone configure/build with `/opt/qnx-python/bin/cmake`, build directory `/home/qnxbuild/systems-probes-linux`, followed by `/opt/qnx-python/bin/ctest --test-dir /home/qnxbuild/systems-probes-linux --output-on-failure`. Ubuntu 24.04 amd64 container under Rosetta, GNU 13.3.0; ASan/UBSan; 3/3 passed. This is not a native Marathon or Wayland result.                                                          |
| Default OFF                        | Passed                  | Standalone configure/build without the option produced zero tests. Root configure with `-DRNQNX_FETCH_REACT_NATIVE_DEPS=OFF -DBUILD_TESTING=OFF` produced no systems target in build help.                                                                                                                                                                                                                                     |
| Root ON                            | Passed                  | Root configure with `-DRNQNX_BUILD_QNX_SYSTEMS_PROBES=ON -DRNQNX_FETCH_REACT_NATIVE_DEPS=OFF -DBUILD_TESTING=OFF`, then `cmake --build build/systems-root --parallel 2 --target rnqnx_systems_probe`.                                                                                                                                                                                                                          |
| Unsupported OS                     | Passed                  | Standalone configure with `-DCMAKE_SYSTEM_NAME=Generic -DRNQNX_BUILD_QNX_SYSTEMS_PROBES=ON` failed with the explicit Linux/Darwin/QNX diagnostic.                                                                                                                                                                                                                                                                              |
| QNX cross-build                    | Passed                  | Activated `/work/qnx800/qnxsdp-env.sh`; `/opt/qnx-python/bin/cmake -S /work/systems-probes-implementation/tests/native/qnx-systems -B /home/qnxbuild/systems-probes-qnx -DCMAKE_TOOLCHAIN_FILE=/work/systems-probes-implementation/cmake/toolchains/qnx-8.0.cmake -DRNQNX_BUILD_QNX_SYSTEMS_PROBES=ON -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF`, then build `--parallel 2`. Licensed SDP 8.0.5, QCC/GCC 12.2.0, AArch64. |
| Physical QNX / QNX VM              | Not run in this receipt | Cross-compilation does not establish target execution, thread behavior or pipe-service availability.                                                                                                                                                                                                                                                                                                                           |
| RN app regression / Marathon shell | Not run                 | Shared application source and dependency graph were unchanged. These standalone host runs do not establish application, graphics or shell behavior.                                                                                                                                                                                                                                                                            |

CTest logs remain in each build's `Testing/Temporary/LastTest.log`. Positive
output was `RNQNX_SYSTEMS_OK threads=2 tls-destructors=2 short-read=14 eof=1 cleanup=1`.
The TLS fault produced `RNQNX_SYSTEMS_FAIL tls-worker-isolation`, exit 1; the
actual early-EOF fault produced `RNQNX_SYSTEMS_FAIL unexpected-eof`, exit 1.
The strict negative wrapper rejects another exit code, missing diagnostic,
success marker, timeout or sanitizer diagnostic.

The QNX ELF is 26,072 bytes, interpreter `/usr/lib/ldqnx-64.so.2`, SHA-256:
`0c64c540fa2347ee5486fd67dddae26247b3305418117a1fd50744591b495f3a`.
It is an application artifact, not a redistributed SDK or Qt evaluation runtime.
The target sysroot and system C++ libraries are not sanitizer-instrumented.
No TSan, broad leak, performance, SMP or real-time guarantee is claimed.

The [run instructions](../../tests/native/qnx-systems/README.md) describe the
three invocations and required external target watchdog. The next target gate
is positive and both injected-negative executions with artifact readback,
health checks and retained output. Kernel short-write, EINTR, cancellation,
resource-manager, scheduler and loader fixtures remain later slices.

## Source and skill decisions

The repository QNX native API and validation skills were applied. Pinned
Margelo sources at `1e9e17be6f41c838db472d6a4b943fc26355a3be` were read from the
existing immutable local cache:

| Skill                 | SHA-256                                                            | Decision                                                                                                            |
| --------------------- | ------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------- |
| `api-design`          | `4c1dc9af65ba69e4716b3cfeee3592e7bc53ed705b13f25d0dfb0569a981ce03` | Keep privileged diagnostics in a native executable; no public JS command surface.                                   |
| `build-nitro-modules` | `57466320611351c5b49737f59dad254c66d0f9ed1d2bac565e532248aaba6185` | No fabricated Nitro binding or version change for a standalone native fixture.                                      |
| `cpp`                 | `556801cd3c68728c7c6cab57d17b4eda68c902803f79d8bb048b3f2a825418e7` | Context/key lifetimes cover worker joins; gate predicates replace sleep-based readiness; descriptors use one owner. |

The owner-requested `no-ai-slop` skill was read for this report. Source reads
are separate from installation and execution. QNX's current
[TLS key reference](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/p/pthread_key_create.html),
[read reference](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/r/read.html)
and [pipe reference](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/p/pipe.html)
were inspected; the QNX compile used the installed SDP headers.

Rollback disables `RNQNX_BUILD_QNX_SYSTEMS_PROBES` and removes only the diagnostic
executable through the normal target procedure. No service or endpoint needs
removal. No credential, privilege or redistribution scope was expanded.
