#!/usr/bin/env node

import { readFile } from 'node:fs/promises';
import path from 'node:path';

const root = process.cwd();

/** @param {string} relative */
async function read(relative) {
  return readFile(path.join(root, relative), 'utf8');
}

/** @param {string} source @param {string} text @param {string} label */
function requireText(source, text, label) {
  if (!source.includes(text)) {
    throw new Error(`${label}: missing ${JSON.stringify(text)}`);
  }
}

const cmake = await read('CMakeLists.txt');
const graph = await read('cmake/RnqnxReactQt.cmake');
const hermesQnx = await read('cmake/dependencies/HermesQNX.cmake');
const renderer = await read('cmake/dependencies/ReactNativeRenderer.cmake');
const probe = await read('vnext/probes/react-qnx/main.cpp');

for (const token of [
  'option(RNQNX_BUILD_REACT_QNX',
  'find_package(Qt6 6.11.1 REQUIRED COMPONENTS Core Gui Qml Quick)',
  'rnqnx_add_qt_views_target()',
  'rnqnx_add_fabric_adapter_target()',
  'rnqnx_add_react_host_target()',
  'rnqnx_add_react_render_probe(rnqnx_react_qnx_probe)',
  'rnqnx_add_react_render_probe(rnqnx_react_render_host_probe)',
]) {
  requireText(cmake, token, 'QNX React target graph');
}

for (const token of [
  'function(rnqnx_add_qt_views_target)',
  'function(rnqnx_add_fabric_adapter_target)',
  'function(rnqnx_add_react_host_target)',
  'rnqnx_react_native_renderer',
  'rnqnx_hermes',
  'AUTOMOC ON',
]) {
  requireText(graph, token, 'shared React/Qt CMake graph');
}

requireText(hermesQnx, 'add_library(jsi INTERFACE)', 'QNX Hermes JSI import');
requireText(hermesQnx, 'target_link_libraries(jsi INTERFACE "${_jsi_a}")', 'QNX Hermes JSI import');
requireText(
  renderer,
  'if(module STREQUAL "jsi" AND TARGET jsi)',
  'React Native renderer JSI deduplication'
);

for (const token of [
  'QGuiApplication app(argc, argv)',
  'QtReactHost host(adapter)',
  'host.hasFabricUIManager()',
  'host.evaluate(',
  'host.startSurface(',
  'host.stopSurface(',
  'assertGeometry(root, false)',
  'assertGeometry(root, true)',
  'host.delegate().mountedTransactionCount()',
  'host.javaScriptErrors()',
  'RNQNX_REACT_RENDER_OK platform=',
  'geometry-only',
]) {
  requireText(probe, token, 'QNX React runtime probe');
}

const probeGraph = await read('cmake/RnqnxReactProbe.cmake');
for (const token of [
  'vnext/probes/react-qnx/main.cpp',
  'react-probe.js --platform qnx --dev false',
  'ReactRenderProbeNegative.cmake',
  'COMMAND $<TARGET_FILE:${target}> -platform offscreen',
]) {
  requireText(probeGraph, token, 'shared bundled React probe');
}
const fixture = await read('apps/rn-playground/react-probe.js');
for (const token of [
  "const React = require('react')",
  "require('react-native/Libraries/Components/View/View')",
  "AppRegistry.registerComponent('RNQnxReactRenderProbe'",
  'setUpdated(true)',
]) {
  requireText(fixture, token, 'real React fixture');
}
if (probe.includes('6 * 7') || probe.includes('startEmptySurface(')) {
  throw new Error('The React probe must exercise a bundled React surface');
}
console.log('validate:qnx-react source contract passed (not runtime evidence)');
