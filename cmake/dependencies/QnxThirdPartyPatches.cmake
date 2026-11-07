include_guard(GLOBAL)
include(${CMAKE_CURRENT_LIST_DIR}/QnxSourcePatches.cmake)

# SDP 8 exposes this POSIX function in the global namespace.
rnqnx_patch_qnx_source("${rnqnx_glog_SOURCE_DIR}/src/logging.cc"
  "#ifdef __QNX__\nusing std::fdopen;\n#endif"
  "#ifdef __QNX__\nusing ::fdopen;\n#endif")

# QNX has no Linux syscall-number interface. Keep Folly's existing portable
# futex path; attempts to use its Linux syscall wrapper report ENOSYS.
rnqnx_patch_qnx_source("${rnqnx_folly_SOURCE_DIR}/folly/portability/SysSyscall.h"
  "#if !defined(_WIN32)\n"
  "#if !defined(_WIN32) && !defined(__QNXNTO__)\n")
rnqnx_patch_qnx_source("${rnqnx_folly_SOURCE_DIR}/folly/portability/SysSyscall.h"
  "#if defined(_WIN32) || (defined(__EMSCRIPTEN__) && !defined(syscall))"
  "#if defined(_WIN32) || defined(__QNXNTO__) || (defined(__EMSCRIPTEN__) && !defined(syscall))")

# SDP 8 declares native socket batch operations; Folly must not redeclare
# their names as fallback function-pointer variables.
rnqnx_patch_qnx_source("${rnqnx_folly_SOURCE_DIR}/folly/net/NetOps.cpp"
  "defined(__FreeBSD__) || defined(__SGX__) || defined(__EMSCRIPTEN__)\n"
  "defined(__FreeBSD__) || defined(__SGX__) || defined(__EMSCRIPTEN__) || defined(__QNXNTO__)\n")

# Use the existing per-message implementation while QNX's native batch path
# remains unverified on the device. Preserve the public API and payloads.
rnqnx_patch_qnx_source("${rnqnx_folly_SOURCE_DIR}/folly/net/NetOps.cpp"
  "  if (to_bool(::recvmmsg)) {\n    return wrapSocketFunction<int>(::recvmmsg, s, msgvec, vlen, flags, timeout);\n  }"
  "#if !defined(__QNXNTO__)\n  if (to_bool(::recvmmsg)) {\n    return wrapSocketFunction<int>(::recvmmsg, s, msgvec, vlen, flags, timeout);\n  }\n#endif")
rnqnx_patch_qnx_source("${rnqnx_folly_SOURCE_DIR}/folly/net/NetOps.cpp"
  "  if (to_bool(::sendmmsg)) {\n    return wrapSocketFunction<int>(::sendmmsg, socket, msgvec, vlen, flags);\n  }"
  "#if !defined(__QNXNTO__)\n  if (to_bool(::sendmmsg)) {\n    return wrapSocketFunction<int>(::sendmmsg, socket, msgvec, vlen, flags);\n  }\n#endif")

# pthread_self is QNX's native thread ID, also used by its procfs thread query.
rnqnx_patch_qnx_source("${rnqnx_folly_SOURCE_DIR}/folly/system/ThreadId.cpp"
  "#elif defined(__FreeBSD__)\n"
  "#elif defined(__QNXNTO__)\n  return uint64_t(pthread_self());\n#elif defined(__FreeBSD__)\n")
