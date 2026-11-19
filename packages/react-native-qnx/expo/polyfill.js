/**
 * QNX stand-in for expo-modules-core's `build/polyfill/index.js`.
 *
 * On iOS and Android the native runtime defines the JavaScript classes that
 * `globalThis.expo` carries (EventEmitter, NativeModule, SharedObject,
 * SharedRef) and the native polyfill entry is a no-op. The QNX host
 * (`installExpoModulesQnx` in @rn-qnx/expo-modules-core-qnx) installs the
 * modules object and the native functions, but a JSI host function cannot be
 * the base of `class X extends NativeModule`, and the lean Hermes build cannot
 * evaluate JavaScript source at startup. So the classes come from here: Metro
 * resolves expo-modules-core's polyfill entry to this file for platform qnx
 * (see @rn-qnx/metro-config-qnx), and expo-modules-core's index imports the
 * polyfill before anything reads `globalThis.expo.EventEmitter`.
 *
 * Without the native host (an app built without Expo modules) this creates
 * `globalThis.expo` with an empty `modules`, so requireNativeModule reports a
 * missing module the same way it does on the web.
 */
'use strict';

class EventEmitter {
  addListener(eventName, listener) {
    if (!this.__listeners) {
      Object.defineProperty(this, '__listeners', { value: new Map(), enumerable: false });
    }
    if (!this.__listeners.has(eventName)) this.__listeners.set(eventName, new Set());
    const before = this.listenerCount(eventName);
    this.__listeners.get(eventName).add(listener);
    if (before === 0 && this.listenerCount(eventName) === 1) this.startObserving(eventName);
    return { remove: () => this.removeListener(eventName, listener) };
  }

  removeListener(eventName, listener) {
    const removed = this.__listeners?.get(eventName)?.delete(listener);
    if (removed && this.listenerCount(eventName) === 0) this.stopObserving(eventName);
  }

  removeAllListeners(eventName) {
    const before = this.listenerCount(eventName);
    this.__listeners?.get(eventName)?.clear();
    if (before > 0) this.stopObserving(eventName);
  }

  emit(eventName, ...args) {
    for (const listener of new Set(this.__listeners?.get(eventName))) {
      // One listener's error must not stop the others (same rule as Expo's).
      try {
        listener(...args);
      } catch (error) {
        console.error(error);
      }
    }
  }

  listenerCount(eventName) {
    return this.__listeners?.get(eventName)?.size ?? 0;
  }

  startObserving() {}

  stopObserving() {}
}

class NativeModule extends EventEmitter {}

class SharedObject extends EventEmitter {
  release() {}
}

class SharedRef extends SharedObject {
  nativeRefType = 'unknown';
}

/**
 * Runs a Fabric view command on the host view behind an Expo view component,
 * for the functions an Expo module puts on a view's prototype (ref.startAnimating()
 * and the like). `component` is the `this` of such a function: the
 * requireNativeView wrapper, whose getNativeRef() is the host view's public
 * instance. React Native's dispatchCommand turns that into
 * UIManager::dispatchCommand, which reaches the mounting layer.
 */
function dispatchViewCommand(component, command, args) {
  const ref = typeof component.getNativeRef === 'function' ? component.getNativeRef() : component;
  if (ref == null) {
    // The view is not mounted; a command has nothing to act on.
    return;
  }
  const { dispatchCommand } = require('react-native/Libraries/ReactNative/RendererProxy');
  dispatchCommand(ref, command, args ?? []);
}

function install() {
  const expo = globalThis.expo ?? (globalThis.expo = { modules: {} });
  expo.modules ??= {};
  expo.EventEmitter ??= EventEmitter;
  expo.NativeModule ??= NativeModule;
  expo.SharedObject ??= SharedObject;
  expo.SharedRef ??= SharedRef;
  expo.dispatchViewCommand ??= dispatchViewCommand;
  // Native modules are plain JSI objects; give each the NativeModule
  // prototype so the event API exists on them, as on iOS and Android.
  for (const name of Object.keys(expo.modules)) {
    const module = expo.modules[name];
    if (module && typeof module === 'object' && !(module instanceof expo.NativeModule)) {
      Object.setPrototypeOf(module, expo.NativeModule.prototype);
      module.__expo_module_name__ ??= name;
    }
  }
}

install();

module.exports = { EventEmitter, NativeModule, SharedObject, SharedRef, dispatchViewCommand };
