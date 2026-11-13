#include "QnxGestureHandlerModule.h"

#include <utility>

namespace rnqnx::gesture {

using facebook::jsi::JSError;
using facebook::jsi::Runtime;
using facebook::jsi::Value;

namespace {

Config toConfig(Runtime& rt, const Value& value) {
  Config config;
  if (!value.isObject()) return config;
  const auto object = value.asObject(rt);
  const auto names = object.getPropertyNames(rt);
  for (size_t i = 0; i < names.size(rt); ++i) {
    const auto name = names.getValueAtIndex(rt, i).getString(rt);
    const auto entry = object.getProperty(rt, name);
    if (entry.isNumber()) {
      config[name.utf8(rt)] = entry.getNumber();
    } else if (entry.isBool()) {
      config[name.utf8(rt)] = entry.getBool() ? 1 : 0;
    }
  }
  return config;
}

std::vector<int> toTags(Runtime& rt, const facebook::jsi::Object& object, const char* key) {
  std::vector<int> tags;
  const auto entry = object.getProperty(rt, key);
  if (!entry.isObject() || !entry.asObject(rt).isArray(rt)) return tags;
  const auto array = entry.asObject(rt).asArray(rt);
  for (size_t i = 0; i < array.size(rt); ++i) {
    const auto element = array.getValueAtIndex(rt, i);
    if (element.isNumber()) tags.push_back(static_cast<int>(element.getNumber()));
  }
  return tags;
}

int toInt(Runtime& rt, const Value* args, size_t count, size_t index, const char* what) {
  if (index >= count || !args[index].isNumber()) {
    throw JSError(rt, std::string("RNGestureHandlerModule: ") + what + " must be a number");
  }
  return static_cast<int>(args[index].getNumber());
}

QnxGestureHandlerModule& self(facebook::react::TurboModule& tm) {
  return static_cast<QnxGestureHandlerModule&>(tm);
}

}  // namespace

QnxGestureHandlerModule::QnxGestureHandlerModule(
    std::shared_ptr<facebook::react::CallInvoker> jsInvoker, Hooks hooks)
    : facebook::react::TurboModule(kModuleName, std::move(jsInvoker)), hooks_(std::move(hooks)) {
  methodMap_["createGestureHandler"] = {3, &QnxGestureHandlerModule::createGestureHandler};
  methodMap_["attachGestureHandler"] = {3, &QnxGestureHandlerModule::attachGestureHandler};
  methodMap_["setGestureHandlerConfig"] = {2, &QnxGestureHandlerModule::setGestureHandlerConfig};
  methodMap_["updateGestureHandlerConfig"] = {2, &QnxGestureHandlerModule::setGestureHandlerConfig};
  methodMap_["configureRelations"] = {2, &QnxGestureHandlerModule::configureRelations};
  methodMap_["dropGestureHandler"] = {1, &QnxGestureHandlerModule::dropGestureHandler};
  methodMap_["flushOperations"] = {0, &QnxGestureHandlerModule::flushOperations};
  methodMap_["installUIRuntimeBindings"] = {0, &QnxGestureHandlerModule::installUIRuntimeBindings};
  methodMap_["getModuleId"] = {0, &QnxGestureHandlerModule::getModuleId};
}

Value QnxGestureHandlerModule::createGestureHandler(Runtime& rt, facebook::react::TurboModule& tm, const Value* args, size_t count) {
  if (count < 3 || !args[0].isString()) throw JSError(rt, "RNGestureHandlerModule: createGestureHandler(name, tag, config)");
  self(tm).hooks_.create(args[0].getString(rt).utf8(rt), toInt(rt, args, count, 1, "handlerTag"), toConfig(rt, args[2]));
  return Value::undefined();
}

Value QnxGestureHandlerModule::attachGestureHandler(Runtime& rt, facebook::react::TurboModule& tm, const Value* args, size_t count) {
  self(tm).hooks_.attach(toInt(rt, args, count, 0, "handlerTag"), toInt(rt, args, count, 1, "viewTag"), toInt(rt, args, count, 2, "actionType"));
  return Value::undefined();
}

Value QnxGestureHandlerModule::setGestureHandlerConfig(Runtime& rt, facebook::react::TurboModule& tm, const Value* args, size_t count) {
  self(tm).hooks_.setConfig(toInt(rt, args, count, 0, "handlerTag"), toConfig(rt, count > 1 ? args[1] : Value::undefined()));
  return Value::undefined();
}

Value QnxGestureHandlerModule::configureRelations(Runtime& rt, facebook::react::TurboModule& tm, const Value* args, size_t count) {
  const int tag = toInt(rt, args, count, 0, "handlerTag");
  Relations relations;
  if (count > 1 && args[1].isObject()) {
    const auto object = args[1].asObject(rt);
    relations.simultaneous = toTags(rt, object, "simultaneousHandlers");
    relations.waitFor = toTags(rt, object, "waitFor");
    relations.blocks = toTags(rt, object, "blocksHandlers");
  }
  self(tm).hooks_.relations(tag, relations);
  return Value::undefined();
}

Value QnxGestureHandlerModule::dropGestureHandler(Runtime& rt, facebook::react::TurboModule& tm, const Value* args, size_t count) {
  self(tm).hooks_.drop(toInt(rt, args, count, 0, "handlerTag"));
  return Value::undefined();
}

Value QnxGestureHandlerModule::flushOperations(Runtime&, facebook::react::TurboModule&, const Value*, size_t) {
  return Value::undefined();
}

Value QnxGestureHandlerModule::installUIRuntimeBindings(Runtime& rt, facebook::react::TurboModule& tm, const Value*, size_t) {
  return Value(self(tm).hooks_.installBindings(rt));
}

Value QnxGestureHandlerModule::getModuleId(Runtime&, facebook::react::TurboModule&, const Value*, size_t) {
  return Value(0);
}

}  // namespace rnqnx::gesture
