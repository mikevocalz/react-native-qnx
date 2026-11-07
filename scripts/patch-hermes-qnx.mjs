#!/usr/bin/env node

import { readFile, writeFile } from 'node:fs/promises';
import path from 'node:path';

const root = process.argv[2];
if (!root) {
  throw new Error('Usage: node scripts/patch-hermes-qnx.mjs <hermes-source-dir>');
}

/**
 * @param {string} file
 * @param {string} before
 * @param {string} after
 * @param {string} label
 */
async function replaceOnce(file, before, after, label) {
  const absolute = path.join(root, file);
  const source = await readFile(absolute, 'utf8');
  const first = source.indexOf(before);
  if (first < 0) {
    throw new Error(`${label}: expected source contract not found in ${file}`);
  }
  if (source.indexOf(before, first + before.length) >= 0) {
    throw new Error(`${label}: source contract is ambiguous in ${file}`);
  }
  const updated = source.slice(0, first) + after + source.slice(first + before.length);
  await writeFile(absolute, updated);
  console.log(`PATCH: ${file} -> ${label}`);
}

await replaceOnce(
  'external/llvh/lib/Support/Unix/Path.inc',
  '#if defined(__NetBSD__) || defined(__EMSCRIPTEN__)\n#define STATVFS_F_FLAG(vfs) (vfs).f_flag',
  '#if defined(__NetBSD__) || defined(__EMSCRIPTEN__) || defined(__QNXNTO__)\n#define STATVFS_F_FLAG(vfs) (vfs).f_flag',
  'QNX statvfs uses f_flag'
);

await replaceOnce(
  'external/llvh/lib/Support/Unix/Path.inc',
  '#elif defined(__EMSCRIPTEN__)\n  return false;\n#else\n  return !!(STATVFS_F_FLAG(Vfs) & MNT_LOCAL);',
  '#elif defined(__EMSCRIPTEN__)\n  return false;\n#elif defined(__QNXNTO__)\n  // QNX has no portable MNT_LOCAL equivalent.\n  return true;\n#else\n  return !!(STATVFS_F_FLAG(Vfs) & MNT_LOCAL);',
  'QNX local-filesystem fallback'
);

await replaceOnce(
  'lib/InternalJavaScript/CMakeLists.txt',
  `# Compile the one source file to a C file.
add_custom_command(
  OUTPUT internal_unit.c
  COMMAND \${shermes_EXE} -O -g0 -w -emit-c -exported-unit=internal_unit -o=\${CMAKE_CURRENT_BINARY_DIR}/internal_unit.c \${CMAKE_CURRENT_BINARY_DIR}/InternalBytecode.js
  DEPENDS \${shermes_EXE} InternalBytecode.js
  WORKING_DIRECTORY \${CMAKE_CURRENT_SOURCE_DIR}
  VERBATIM
)

# Native compiled InternalJavaScript target
# Always build it to prevent bitrot, except on MSVC where the generated C code
# is not compatible with MSVC's C compiler.
if(HERMESVM_INTERNAL_JAVASCRIPT_NATIVE OR NOT MSVC)
  add_hermes_library(hermesInternalUnit internal_unit.c)
  target_compile_options(hermesInternalUnit_obj PRIVATE -w)
  target_include_directories(hermesInternalUnit_obj PRIVATE \${CMAKE_CURRENT_BINARY_DIR})
endif()`,
  `# Native compiled InternalJavaScript target.
# QNX cross-builds use bytecode InternalJavaScript because the npm host compiler
# package ships hermesc but not shermes.
if(HERMESVM_INTERNAL_JAVASCRIPT_NATIVE OR
   (NOT MSVC AND NOT CMAKE_SYSTEM_NAME STREQUAL "QNX"))
  add_custom_command(
    OUTPUT internal_unit.c
    COMMAND \${shermes_EXE} -O -g0 -w -emit-c -exported-unit=internal_unit -o=\${CMAKE_CURRENT_BINARY_DIR}/internal_unit.c \${CMAKE_CURRENT_BINARY_DIR}/InternalBytecode.js
    DEPENDS \${shermes_EXE} InternalBytecode.js
    WORKING_DIRECTORY \${CMAKE_CURRENT_SOURCE_DIR}
    VERBATIM
  )
  add_hermes_library(hermesInternalUnit internal_unit.c)
  target_compile_options(hermesInternalUnit_obj PRIVATE -w)
  target_include_directories(hermesInternalUnit_obj PRIVATE \${CMAKE_CURRENT_BINARY_DIR})
endif()`,
  'skip shermes-only native InternalJavaScript on QNX'
);

await replaceOnce(
  'lib/Support/OSCompatPosix.cpp',
  `static constexpr int kVMReserveProt = PROT_NONE;
static constexpr int kVMReserveFlags =
    MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE;`,
  `static constexpr int kVMReserveProt = PROT_NONE;
#if defined(__QNXNTO__) && !defined(MAP_NORESERVE)
#define MAP_NORESERVE 0
#endif
static constexpr int kVMReserveFlags =
    MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE;`,
  'QNX MAP_NORESERVE fallback'
);

await replaceOnce(
  'lib/Support/OSCompatPosix.cpp',
  `#elif defined(__linux__)

/// On linux, telling the OS that we \\p MADV_DONTNEED some pages will cause it
/// to immediately deduct their size from the process's resident set.
#define MADV_UNUSED MADV_DONTNEED

#else
#error "Don't know how to return memory to the OS on this platform."
#endif // __MACH__, __linux__

  madvise(p, sz, MADV_UNUSED);

#undef MADV_UNUSED`,
  `#elif defined(__linux__)

/// On linux, telling the OS that we \\p MADV_DONTNEED some pages will cause it
/// to immediately deduct their size from the process's resident set.
#define MADV_UNUSED MADV_DONTNEED

#elif defined(__QNXNTO__)
  (void)p;
  (void)sz;
  return;

#else
#error "Don't know how to return memory to the OS on this platform."
#endif // __MACH__, __linux__, __QNXNTO__

#if !defined(__QNXNTO__)
  madvise(p, sz, MADV_UNUSED);

#undef MADV_UNUSED
#endif`,
  'QNX vm_unused fallback'
);

await replaceOnce(
  'lib/Support/OSCompatPosix.cpp',
  `void vm_prefetch(void *p, size_t sz) {
  assert(
      reinterpret_cast<intptr_t>(p) % page_size() == 0 &&
      "Precondition: pointer is page-aligned.");
  madvise(p, sz, MADV_WILLNEED);
}`,
  `void vm_prefetch(void *p, size_t sz) {
  assert(
      reinterpret_cast<intptr_t>(p) % page_size() == 0 &&
      "Precondition: pointer is page-aligned.");
#if defined(__QNXNTO__)
  (void)p;
  (void)sz;
#else
  madvise(p, sz, MADV_WILLNEED);
#endif
}`,
  'QNX vm_prefetch fallback'
);

await replaceOnce(
  'lib/Support/OSCompatPosix.cpp',
  `  int param = MADV_NORMAL;
  switch (advice) {
    case MAdvice::Random:
      param = MADV_RANDOM;
      break;
    case MAdvice::Sequential:
      param = MADV_SEQUENTIAL;
      break;
  }
  return madvise(p, sz, param) == 0;`,
  `#if defined(__QNXNTO__)
  (void)p;
  (void)sz;
  (void)advice;
  return true;
#else
  int param = MADV_NORMAL;
  switch (advice) {
    case MAdvice::Random:
      param = MADV_RANDOM;
      break;
    case MAdvice::Sequential:
      param = MADV_SEQUENTIAL;
      break;
  }
  return madvise(p, sz, param) == 0;
#endif`,
  'QNX vm_madvise fallback'
);

await replaceOnce(
  'lib/Support/OSCompatPosix.cpp',
  '#else\n  auto rStart = reinterpret_cast<uintptr_t>(start);',
  '#elif defined(__QNXNTO__)\n  (void)start;\n  (void)end;\n  return size_t{0};\n#else\n  auto rStart = reinterpret_cast<uintptr_t>(start);',
  'QNX vm_footprint fallback'
);

await replaceOnce(
  'lib/Support/OSCompatPosix.cpp',
  'int pages_in_ram(const void *p, size_t sz, llvh::SmallVectorImpl<int> *runs) {\n  const auto PS = page_size();',
  'int pages_in_ram(const void *p, size_t sz, llvh::SmallVectorImpl<int> *runs) {\n#ifdef __QNXNTO__\n  (void)p;\n  (void)sz;\n  (void)runs;\n  return -1;\n#else\n  const auto PS = page_size();',
  'QNX pages_in_ram fallback'
);

await replaceOnce(
  'lib/Support/OSCompatPosix.cpp',
  '  return totalIn;\n}\n\nuint64_t peak_rss()',
  '  return totalIn;\n#endif\n}\n\nuint64_t peak_rss()',
  'close QNX pages_in_ram guard'
);

await replaceOnce(
  'lib/Support/OSCompatPosix.cpp',
  '#else\n  FILE *fp = fopen("/proc/self/statm", "r");',
  '#elif defined(__QNXNTO__)\n  return 0;\n#else\n  FILE *fp = fopen("/proc/self/statm", "r");',
  'QNX current_rss fallback'
);

await replaceOnce(
  'lib/Support/OSCompatPosix.cpp',
  `#elif defined(__linux__)

uint64_t global_thread_id() {
  return syscall(__NR_gettid);
}

#else
#error "Thread ID not supported on this platform"
#endif`,
  `#elif defined(__linux__)

uint64_t global_thread_id() {
  return syscall(__NR_gettid);
}

#elif defined(__QNXNTO__)

uint64_t global_thread_id() {
  return 0;
}

#else
#error "Thread ID not supported on this platform"
#endif`,
  'QNX global thread id fallback'
);

await replaceOnce(
  'lib/Support/OSCompatPosix.cpp',
  '#include <sys/resource.h>',
  `#include <sys/resource.h>
#if defined(__QNXNTO__)
#include <cstdint>
#include <devctl.h>
#include <fcntl.h>
#include <sys/procfs.h>
#endif`,
  'QNX thread status headers'
);

await replaceOnce(
  'lib/Support/OSCompatPosix.cpp',
  '#else\n\nstd::pair<const void *, size_t> thread_stack_bounds_impl() {\n  pthread_attr_t attr;',
  `#elif defined(__QNXNTO__)

std::pair<const void *, size_t> thread_stack_bounds_impl() {
  // A nested function's frame is below its caller on QNX AArch64. It cannot
  // serve as the stack high address: the guard's unsigned subtraction wraps.
  const int fd = ::open("/proc/self/ctl", O_RDONLY);
  if (fd == -1)
    hermes_fatal("Unable to open current QNX thread status");
  procfs_status status{};
  const auto tid = ::pthread_self();
  status.tid = tid;
  const int result =
      ::devctl(fd, DCMD_PROC_TIDSTATUS, &status, sizeof(status), nullptr);
  ::close(fd);
  const uintptr_t low = static_cast<uintptr_t>(status.stkbase);
  const size_t size = status.stksize;
  const uintptr_t high = low + size;
  const uintptr_t frame =
      reinterpret_cast<uintptr_t>(__builtin_frame_address(0));
  if (result != 0 || status.tid != tid || size == 0 || high <= low ||
      frame < low || frame >= high)
    hermes_fatal("Unable to determine current QNX thread stack bounds");
  return {reinterpret_cast<const void *>(high), size};
}

#else

std::pair<const void *, size_t> thread_stack_bounds_impl() {
  pthread_attr_t attr;`,
  'QNX actual thread stack bounds'
);

await replaceOnce(
  'lib/Support/OSCompatPosix.cpp',
  `#elif defined(__APPLE__)
  ::pthread_setname_np(name);
#endif`,
  `#elif defined(__APPLE__)
  ::pthread_setname_np(name);
#elif defined(__QNXNTO__)
  ::pthread_setname_np(pthread_self(), name);
#endif`,
  'QNX thread naming'
);

await replaceOnce(
  'lib/Support/OSCompatPosix.cpp',
  `#else // !(__APPLE__ && __MACH__), !__linux__
#error "Thread CPU Time not supported on this platform"
#endif // thread_cpu_time: (__APPLE__ && __MACH__), __linux__`,
  `#elif defined(__QNXNTO__)

std::chrono::microseconds thread_cpu_time() {
  return std::chrono::microseconds::max();
}

#else // !(__APPLE__ && __MACH__), !__linux__, !__QNXNTO__
#error "Thread CPU Time not supported on this platform"
#endif // thread_cpu_time: (__APPLE__ && __MACH__), __linux__, __QNXNTO__`,
  'QNX thread CPU time fallback'
);

await replaceOnce(
  'lib/Support/OSCompatPosix.cpp',
  `#else // !(__APPLE__ && __MACH__), !__linux__
#error "Thread page fault count not supported on this platform"
#endif // thread_page_fault_count: (__APPLE__ && __MACH__), __linux__`,
  `#elif defined(__QNXNTO__)

bool thread_page_fault_count(int64_t *outMinorFaults, int64_t *outMajorFaults) {
  *outMinorFaults = 0;
  *outMajorFaults = 0;
  return false;
}

#else // !(__APPLE__ && __MACH__), !__linux__, !__QNXNTO__
#error "Thread page fault count not supported on this platform"
#endif // thread_page_fault_count: (__APPLE__ && __MACH__), __linux__, __QNXNTO__`,
  'QNX thread page-fault fallback'
);

await replaceOnce(
  'lib/VM/Instrumentation/ProcessStats.cpp',
  '#elif defined(__EMSCRIPTEN__)\n  rss = va = 0;',
  '#elif defined(__EMSCRIPTEN__) || defined(__QNXNTO__)\n  rss = va = 0;',
  'QNX process statistics fallback'
);

await replaceOnce(
  'lib/VM/Profiler/SamplingProfilerPosix.cpp',
  `  // Allows interrupted IO primitives to restart.
  actions.sa_flags = SA_RESTART;`,
  `  // Allows interrupted IO primitives to restart when the platform exposes it.
#ifdef SA_RESTART
  actions.sa_flags = SA_RESTART;
#endif`,
  'guard SA_RESTART for QNX'
);

console.log('Hermes QNX source transforms completed.');
