#pragma once

#include <jsi/jsi.h>

#include <string>

namespace rnqnx::nitro {

// JavaScript the Marathon host evaluates after installing Nitro when started
// with --nitro-proof. It constructs MarathonPlatformInfo through the installed
// NitroModulesProxy and stores what it read on globalThis.
extern const char* const kMarathonNitroProofSource;

// Reads the values the proof script stored and checks them against the native
// class: platform "marathon", add(2, 3) == 5, and the architecture string the
// native side reports. Returns the marker line text, or throws
// std::runtime_error naming the first mismatch.
std::string verifyMarathonNitroProof(facebook::jsi::Runtime& runtime);

} // namespace rnqnx::nitro
