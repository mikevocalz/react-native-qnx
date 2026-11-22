#include <NitroModules/HybridClass.hpp>
#include <NitroModules/HybridObject.hpp>
#include <NitroModules/InstallNitro.hpp>
#include <hermes/hermes.h>
#include <jsi/jsi.h>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#ifndef NDEBUG
#error "Receiver safety regression must compile its bindings with NDEBUG"
#endif
#ifdef NITRO_DEBUG
#error "Receiver safety regression must exercise Nitro's release helper"
#endif

namespace {
using margelo::nitro::HybridClass;
using margelo::nitro::HybridObject;
// Non-final abstract specs follow the same receiver path as generated WindowSpec.
class ReceiverSpec : public HybridObject {
 public:
  struct Class {
    using Base = HybridObject::Class;
    static constexpr const char* name = "ReleaseReceiver";
    static const std::shared_ptr<const HybridClass>& get();
  };
  virtual double value() = 0;
 protected:
  const std::shared_ptr<const HybridClass>& getHybridClass() override { return Class::get(); }
};
const std::shared_ptr<const HybridClass>& ReceiverSpec::Class::get() {
  static const auto descriptor = HybridClass::create<Class>([](HybridClass& cls) {
    cls.registerHybridProperty("value", &ReceiverSpec::value);
    cls.registerHybridMethod("read", &ReceiverSpec::value);
  });
  return descriptor;
}
class Receiver final : public ReceiverSpec {
 public:
  double value() override { return 42; }
};
class OtherNativeState final : public facebook::jsi::NativeState {};
} // namespace

int main() {
  try {
    auto runtime = facebook::hermes::makeHermesRuntime();
    margelo::nitro::install(*runtime);
    auto first = std::make_shared<Receiver>();
    auto second = std::make_shared<Receiver>();
    runtime->global().setProperty(*runtime, "first", first->toObject(*runtime));
    runtime->global().setProperty(*runtime, "second", second->toObject(*runtime));
    facebook::jsi::Object wrong(*runtime);
    wrong.setNativeState(*runtime, std::make_shared<OtherNativeState>());
    runtime->global().setProperty(*runtime, "wrong", std::move(wrong));
    const std::string source = R"JS(
      const check = (ok, message) => { if (!ok) throw new Error(message); };
      const rejects = (run, message) => {
        let rejected = false;
        try { run(); } catch (error) { rejected = error instanceof Error; }
        check(rejected, message);
      };
      check(first.value === 42 && first.read() === 42, 'valid derived receiver failed');
      const getter = Object.getOwnPropertyDescriptor(Object.getPrototypeOf(first), 'value').get;
      const detached = first.read;
      // Keep the VM crash's exact dispose -> getter sequence first, so an
      // unpatched release helper fails here rather than at a later control.
      first.dispose();
      rejects(() => first.value, 'disposed getter did not reject');
      rejects(() => first.read(), 'disposed method did not reject');
      rejects(() => detached(), 'detached method did not reject');
      rejects(() => getter.call(undefined), 'undefined receiver did not reject');
      rejects(() => getter.call({}), 'plain forged receiver did not reject');
      rejects(() => getter.call(wrong), 'wrong NativeState type did not reject');
      check(second.value === 42, 'disposing first corrupted independent receiver');
      second.dispose();
    )JS";
    runtime->evaluateJavaScript(std::make_unique<facebook::jsi::StringBuffer>(source),
                               "nitro-release-receivers.js");
    std::cout << "NITRO_RELEASE_RECEIVERS_OK NDEBUG valid detached forged wrong-type disposed getter method independent\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "NITRO_RELEASE_RECEIVERS_FAIL " << error.what() << '\n';
    return 1;
  }
}
