/*
 * The app key rnqnx-winmgr groups windows by: the program name without its
 * directory and without build-variant suffixes. A development client and the
 * release build of one app run the same program name, or one with a variant
 * suffix, and are the same app: one dock entry, and a second window of the
 * same app is a duplicate. Keep the suffix list in step with
 * buildVariantSuffixes() in native/window-chrome/WindowHost.cpp and base_id()
 * in template/qnx/run-app.sh.
 *
 * No Screen dependency: tests/js/qnx-winmgr-appkey.test.mjs compiles this on
 * the host.
 */
#ifndef RNQNX_WINMGR_APPKEY_H
#define RNQNX_WINMGR_APPKEY_H

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static const char* const rnqnx_variant_suffixes[] = {".dev", ".debug", ".preview", NULL};

/* Writes the key for a program name into out. A name that is not a program
 * ("Window 123" is what process_name falls back to) gets an empty key and is
 * never grouped. */
static inline void rnqnx_app_key(const char* name, char* out, size_t len) {
  out[0] = 0;
  if (!name || !name[0] || strncmp(name, "Window ", 7) == 0) return;
  const char* base = strrchr(name, '/');
  snprintf(out, len, "%s", base ? base + 1 : name);
  for (bool stripped = true; stripped;) {
    stripped = false;
    size_t n = strlen(out);
    int dots = 0;
    for (size_t i = 0; i < n; ++i) dots += out[i] == '.';
    for (int i = 0; rnqnx_variant_suffixes[i]; ++i) {
      size_t s = strlen(rnqnx_variant_suffixes[i]);
      /* At least two dotted parts stay: "org.dev" is not a variant of "org". */
      if (n > s && dots >= 2 && strcmp(out + n - s, rnqnx_variant_suffixes[i]) == 0) {
        out[n - s] = 0;
        stripped = true;
        break;
      }
    }
  }
}

static inline bool rnqnx_same_app(const char* a, const char* b) {
  return a[0] && b[0] && strcmp(a, b) == 0;
}

#endif
