#ifndef QNX_HERMES_MACHINE_ENDIAN_SHIM_H
#define QNX_HERMES_MACHINE_ENDIAN_SHIM_H

/* QNX 8.0 does not ship <machine/endian.h> in the public application sysroot.
 * Hermes/LLVM use it only to derive BYTE_ORDER. This shim uses GCC builtins so
 * it works for both x86_64 and aarch64 little-endian QNX targets. */

#define BIG_ENDIAN 4321
#define LITTLE_ENDIAN 1234

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define BYTE_ORDER BIG_ENDIAN
#else
#define BYTE_ORDER LITTLE_ENDIAN
#endif

#endif
