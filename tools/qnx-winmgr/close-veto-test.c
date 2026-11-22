#include "close-veto.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
  int a, b, other;
  rnqnx_closing_t records[3] = {{&a, 41, {100, 2}}, {&b, 42, {101, 3}}};
  int count = 2;
  assert(!rnqnx_apply_close_veto(records, &count, NULL, RNQNX_CLOSE_VETO_ACK));
  assert(!rnqnx_apply_close_veto(records, &count, &other, RNQNX_CLOSE_VETO_ACK));
  assert(!rnqnx_apply_close_veto(records, &count, &a, "rnqnx:close-cancelled=other-window"));
  assert(!rnqnx_apply_close_veto(records, &count, &a, "rnqnx-winmgr windowed close-veto"));
  assert(count == 2);
  assert(rnqnx_apply_close_veto(records, &count, &a, RNQNX_CLOSE_VETO_ACK));
  assert(count == 1 && records[0].win == &b && records[0].pid == 42);
  assert(records[0].deadline.tv_sec == 101 && records[0].deadline.tv_nsec == 3);
  assert(!rnqnx_apply_close_veto(records, &count, &a, RNQNX_CLOSE_VETO_ACK));
  assert(count == 1);
  assert(rnqnx_apply_close_veto(records, &count, &b, RNQNX_CLOSE_VETO_ACK));
  assert(count == 0);
  puts("close-veto: ok matching property-window only, no payload-selected identity");
  return 0;
}
