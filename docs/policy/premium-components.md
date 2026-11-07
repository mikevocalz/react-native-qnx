# Optional premium components

Nitro Forge is a separately sold premium component product. React Native QNX provides generic native runtime, rendering, Nitro v1 module integration, package discovery/linking, lifecycle and platform services so licensed developers can install selected Forge packages in their own apps with minimal setup. Nitro Modules is infrastructure; Nitro Forge is a separate commercial product.

Do not make Forge a built-in platform/system component, default dependency, bundled sample requirement or prerequisite for platform build, boot, tests or release. Do not vendor premium components, engines, assets, binaries or entitlement code into platform distributions. Forge-specific production adapters and package configuration belong with Forge or the consuming app; generic reusable hooks belong here. Preserve existing product licensing and distribution terms.

The current default-OFF external-checkout adapter is a compatibility experiment, not permission to redistribute Forge or proof of seamless installation. Keep historical evidence; do not duplicate engines or remove working probes merely to change ownership boundaries. New integration must support install selected licensed package → import → build/run through documented tooling, without patching platform sources or hand-wiring each component. Verify ABI alignment, linkage, input, lifecycle, update and uninstall in an authorized consuming app. Core CI uses non-premium fixtures; commercial integration tests are explicitly opt-in and separately authorized. Removing Forge must leave the platform usable. Packaging must exclude external premium source/build outputs unless producing an expressly licensed consuming-app artifact.

These rules apply to QNX and the Marathon native target and override earlier roadmap language implying Forge is a platform component or required conformance workload. Compatibility remains a measured acceptance goal, not a shipped feature claim.

See [the Forge compatibility experiment](../native-modules/forge-rust-qnx.md).
