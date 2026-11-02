#include <NitroModules/ThreadUtils.hpp>

#include <pthread.h>
#include <stdexcept>

namespace margelo::nitro {

std::string ThreadUtils::getThreadName() {
  char name[64] = {};
  if (::pthread_getname_np(::pthread_self(), name, sizeof(name)) != 0) {
    return {};
  }
  return name;
}

void ThreadUtils::setThreadName(const std::string& name) {
  ::pthread_setname_np(::pthread_self(), name.c_str());
}

bool ThreadUtils::isUIThread() {
  // The QNX React/Qt event-loop dispatcher is a separate integration seam.
  // Returning false prevents Nitro from constructing a fake UI dispatcher.
  return false;
}

std::shared_ptr<Dispatcher> ThreadUtils::createUIThreadDispatcher() {
  throw std::runtime_error(
      "QNX Nitro UI dispatcher is not installed. Use synchronous HybridObjects "
      "until the React/Qt runtime supplies the real dispatcher.");
}

} // namespace margelo::nitro
