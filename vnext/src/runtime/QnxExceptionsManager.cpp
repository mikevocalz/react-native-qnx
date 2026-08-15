#include "react-native-qnx/runtime/QnxExceptionsManager.h"

#include <utility>

namespace rnqnx::runtime {
namespace {

/// Reads an optional string property, returning an empty string when the
/// property is absent or not a string. React Native's ExceptionData marks
/// several fields optional, so a missing one is expected rather than an error.
std::string stringProperty(
    facebook::jsi::Runtime& runtime,
    const facebook::jsi::Object& object,
    const char* name) {
  const auto value = object.getProperty(runtime, name);
  if (!value.isString()) {
    return {};
  }
  return value.getString(runtime).utf8(runtime);
}

/// Flattens React Native's stack frame array into text. Each frame is an object
/// with methodName, file, lineNumber and column; the shape comes from
/// `Libraries/Core/NativeExceptionsManager.js`.
std::string flattenStack(
    facebook::jsi::Runtime& runtime,
    const facebook::jsi::Array& stack) {
  std::string text;
  const size_t count = stack.size(runtime);
  for (size_t index = 0; index < count; ++index) {
    const auto frame = stack.getValueAtIndex(runtime, index);
    if (!frame.isObject()) {
      continue;
    }
    const auto frameObject = frame.getObject(runtime);
    const auto method = stringProperty(runtime, frameObject, "methodName");
    const auto file = stringProperty(runtime, frameObject, "file");
    const auto line = frameObject.getProperty(runtime, "lineNumber");

    text += "  at ";
    text += method.empty() ? "<anonymous>" : method;
    if (!file.empty()) {
      text += " (";
      text += file;
      if (line.isNumber()) {
        text += ':';
        text += std::to_string(static_cast<long long>(line.getNumber()));
      }
      text += ')';
    }
    text += '\n';
  }
  return text;
}

}  // namespace

QnxExceptionsManager::QnxExceptionsManager(
    std::shared_ptr<facebook::react::CallInvoker> jsInvoker,
    Reporter reporter)
    : NativeExceptionsManagerCxxSpec(std::move(jsInvoker)),
      reporter_(std::move(reporter)) {}

void QnxExceptionsManager::reportFatalException(
    facebook::jsi::Runtime& runtime,
    facebook::jsi::String message,
    facebook::jsi::Array stack,
    double /*exceptionId*/) {
  reporter_(JavaScriptError{
      .message = message.utf8(runtime),
      .stack = flattenStack(runtime, stack),
      .isFatal = true});
}

void QnxExceptionsManager::reportSoftException(
    facebook::jsi::Runtime& runtime,
    facebook::jsi::String message,
    facebook::jsi::Array stack,
    double /*exceptionId*/) {
  reporter_(JavaScriptError{
      .message = message.utf8(runtime),
      .stack = flattenStack(runtime, stack),
      .isFatal = false});
}

void QnxExceptionsManager::reportException(
    facebook::jsi::Runtime& runtime,
    facebook::jsi::Object data) {
  const auto isFatalValue = data.getProperty(runtime, "isFatal");
  const auto stackValue = data.getProperty(runtime, "stack");

  JavaScriptError error{
      .message = stringProperty(runtime, data, "message"),
      .stack = {},
      .isFatal = isFatalValue.isBool() && isFatalValue.getBool()};

  if (stackValue.isObject() && stackValue.getObject(runtime).isArray(runtime)) {
    error.stack =
        flattenStack(runtime, stackValue.getObject(runtime).getArray(runtime));
  }

  reporter_(error);
}

void QnxExceptionsManager::dismissRedbox(
    facebook::jsi::Runtime& /*runtime*/) {
  // There is no redbox on this platform, so there is nothing to dismiss. This
  // is an honest no-op rather than an unimplemented method: React Native calls
  // it unconditionally when an error is cleared.
}

QnxSourceCode::QnxSourceCode(
    std::shared_ptr<facebook::react::CallInvoker> jsInvoker,
    std::string scriptUrl)
    : NativeSourceCodeCxxSpec(std::move(jsInvoker)),
      scriptUrl_(std::move(scriptUrl)) {}

facebook::jsi::Object QnxSourceCode::getConstants(
    facebook::jsi::Runtime& runtime) {
  auto constants = facebook::jsi::Object(runtime);
  constants.setProperty(
      runtime,
      "scriptURL",
      facebook::jsi::String::createFromUtf8(runtime, scriptUrl_));
  return constants;
}

}  // namespace rnqnx::runtime
