#ifndef RNQNX_CLOSE_VETO_H
#define RNQNX_CLOSE_VETO_H
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#define RNQNX_CLOSE_VETO_ACK "rnqnx:close-cancelled"
/* Window identity comes from a Screen PROPERTY event, never a payload field.
 * MANAGER_STRING is owner-writable, read-only for other clients by default. */
typedef struct {
  void* win;
  pid_t pid;
  struct timespec deadline;
} rnqnx_closing_t;

static inline bool rnqnx_apply_close_veto(rnqnx_closing_t* records, int* count,
                                         void* property_window, const char* value) {
  if (!property_window || strcmp(value, RNQNX_CLOSE_VETO_ACK) != 0) return false;
  for (int i = 0; i < *count; ++i) {
    if (records[i].win != property_window) continue;
    memmove(&records[i], &records[i + 1], (size_t)(*count - i - 1) * sizeof records[0]);
    --*count;
    return true;
  }
  return false;
}
#endif
