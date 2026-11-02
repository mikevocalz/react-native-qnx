/**
 * Minimal sync QNX-only Nitro v1 proof contract.
 *
 * The C++ HybridClass is registered with NitroModulesProxy by the QNX native
 * host before JS import. This API is NOT a QNX/Marathon generic Nitro runtime.
 * It is a synchronous native binding proof; async callbacks, codegen for QNX,
 * and lifecycle controls are not implemented.
 */
export interface QnxPlatformInfo {
  /** OS identity produced by the native QNX runtime. */
  readonly platform: 'qnx';
  /** CPU architecture reported by the native runtime. */
  readonly architecture: string;
  /** Synchronous deterministic test of the v1 HybridClass binding. */
  add(left: number, right: number): number;
}

/**
 * Create the QNX native binding after installQnxNitroRuntime has run.
 *
 * Throws if the Nitro runtime is missing or the QNX HybridClass is not
 * registered. A Linux/Marathon host must not use this QNX-only constructor.
 */
export function createQnxPlatformInfo(): QnxPlatformInfo;
