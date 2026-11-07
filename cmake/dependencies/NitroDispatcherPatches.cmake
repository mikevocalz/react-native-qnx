include_guard(GLOBAL)
include(${CMAKE_CURRENT_LIST_DIR}/QnxSourcePatches.cmake)

# Pinned v1 admission failures must settle native awaitables and must not escape
# fire-and-forget callbacks. Accepted tasks are drained before runtime teardown.
rnqnx_patch_qnx_source(
  "${RNQNX_NITRO_CORE_ROOT}/native/cpp/include/NitroModules/Dispatcher.hpp"
  [=[    runAsync([function = std::forward<Func>(function), promise]() mutable {
      try {
        if constexpr (std::is_pointer_v<std::decay_t<Func>>) {
          if (function == nullptr) {
            throw std::bad_function_call();
          }
        }
        if constexpr (std::is_void_v<T>) {
          // 4. Call the actual function on the new Thread
          std::invoke(function);
          // 5.a. Resolve the Promise if we succeeded
          promise.resolve();
        } else {
          // 4. Call the actual function on the new Thread
          T result = std::invoke(function);
          // 5.a. Resolve the Promise if we succeeded
          promise.resolve(std::move(result));
        }
      } catch (...) {
        // 5.b. Reject the Promise if the call failed
        promise.reject(std::current_exception());
      }
    });]=]
  [=[    try {
      runAsync([function = std::forward<Func>(function), promise]() mutable {
        try {
          if constexpr (std::is_pointer_v<std::decay_t<Func>>) {
            if (function == nullptr) {
              throw std::bad_function_call();
            }
          }
          if constexpr (std::is_void_v<T>) {
            // 4. Call the actual function on the new Thread
            std::invoke(function);
            // 5.a. Resolve the Promise if we succeeded
            promise.resolve();
          } else {
            // 4. Call the actual function on the new Thread
            T result = std::invoke(function);
            // 5.a. Resolve the Promise if we succeeded
            promise.resolve(std::move(result));
          }
        } catch (...) {
          // 5.b. Reject the Promise if the call failed
          promise.reject(std::current_exception());
        }
      });
    } catch (...) {
      promise.reject(std::current_exception());
    }]=])

rnqnx_patch_qnx_source(
  "${RNQNX_NITRO_CORE_ROOT}/native/cpp/include/NitroModules/Callback.hpp"
  [=[      dispatcher->runAsync([&runtime, function = _function, ... args = std::move(args)]() mutable -> void {
        try {
          assertFunctionNotNull(function);

          function->call(runtime, JSIConverter<std::decay_t<Args>>::toJSI(runtime, std::forward<Args>(args))...);
        } catch (...) {
          GlobalErrorHandler::reportError(ErrorSource::JSCallback, std::current_exception());
        }
      });]=]
  [=[      try {
        dispatcher->runAsync([&runtime, function = _function, ... args = std::move(args)]() mutable -> void {
          try {
            assertFunctionNotNull(function);

            function->call(runtime, JSIConverter<std::decay_t<Args>>::toJSI(runtime, std::forward<Args>(args))...);
          } catch (...) {
            GlobalErrorHandler::reportError(ErrorSource::JSCallback, std::current_exception());
          }
        });
      } catch (...) {
        GlobalErrorHandler::reportError(ErrorSource::JSCallback, std::current_exception());
      }]=])
