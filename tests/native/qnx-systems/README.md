# Finite thread, TLS and pipe diagnostics

This native executable checks two worker identities, a predicate-based startup
handshake, independent POSIX TLS slots, thread-exit TLS destructors, nonblocking
pipe admission, an actual short read, EOF and descriptor cleanup. It does not
expose scheduling, file access or process control to JavaScript.

The fixture makes no scheduling or latency claim. It does not test kernel short
writes, interrupted I/O, cancellation, priority inversion or every Plan 14 case.
The shared RN/Qt/Hermes runtime and existing probes remain unchanged.

The root and standalone CMake projects accept the default-OFF option
`RNQNX_BUILD_QNX_SYSTEMS_PROBES`. The standalone project needs only C++20 and
POSIX threads, so it can be cross-built without RN, Qt or premium packages.
Linux and macOS execution is portable host evidence; only execution on QNX
establishes a QNX result.

```sh
cmake -S tests/native/qnx-systems -B build/systems-host \
  -DRNQNX_BUILD_QNX_SYSTEMS_PROBES=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build/systems-host --parallel 2
ctest --test-dir build/systems-host --output-on-failure
```

Host sanitizers default ON and instrument all three fixture translation units
with ASan/UBSan on Clang/GNU. System libraries are not rebuilt with sanitizers.
CTest bounds the positive executable at ten seconds and strictly verifies the
negative cases. A signal, timeout, unrelated failure, missing diagnostic or
sanitizer report cannot satisfy a negative test.

For QNX, activate the licensed SDP environment first, then supply the absolute
path to the existing toolchain file:

```sh
cmake -S tests/native/qnx-systems -B build/systems-qnx \
  -DCMAKE_TOOLCHAIN_FILE=/absolute/repository/cmake/toolchains/qnx-8.0.cmake \
  -DQNX_TARGET_ARCH=aarch64 -DRNQNX_BUILD_QNX_SYSTEMS_PROBES=ON \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build/systems-qnx --parallel 2
```

Deploy only the resulting `bin/rnqnx_systems_probe` through the authorized target
procedure. Run each target invocation under an external supervisor with a finite
deadline and owned-child cleanup. The startup predicate has a three-second wait;
worker joins have no independent deadline. An outer watchdog remains required
if a platform primitive fails to return. No boot or graphics change is needed.

The no-argument run must exit zero and print:

```text
RNQNX_SYSTEMS_OK threads=2 tls-destructors=2 short-read=14 eof=1 cleanup=1
```

`--inject tls-mismatch` clears one worker's actual TLS slot and must exit one
with `RNQNX_SYSTEMS_FAIL tls-worker-isolation`. `--inject early-eof` requests
one more byte after the writer closes and must exit one with
`RNQNX_SYSTEMS_FAIL unexpected-eof`. Both paths release owned native resources
before main reports failure. Invalid arguments exit 64.

Disable the option to remove this diagnostic target. It installs no services,
creates no filesystem endpoint and changes no application dependency or ABI.
