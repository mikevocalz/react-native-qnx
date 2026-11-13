/* Host test for appkey.h: cc -Wall -Wextra -Werror appkey-test.c && ./a.out */
#include "appkey.h"

#include <assert.h>

static void key(const char* name, const char* expected) {
  char out[128];
  rnqnx_app_key(name, out, sizeof out);
  if (strcmp(out, expected) != 0) {
    fprintf(stderr, "key(%s) = '%s', expected '%s'\n", name, out, expected);
    assert(0);
  }
}

int main(void) {
  key("org.rnqnx.showcase", "org.rnqnx.showcase");
  key("./org.rnqnx.showcase", "org.rnqnx.showcase");
  key("/data/home/qnxuser/app/org.rnqnx.showcase", "org.rnqnx.showcase");
  key("org.rnqnx.showcase.dev", "org.rnqnx.showcase");
  key("org.rnqnx.showcase.debug", "org.rnqnx.showcase");
  key("org.rnqnx.showcase.preview", "org.rnqnx.showcase");
  key("org.rnqnx.showcase.dev.debug", "org.rnqnx.showcase");
  key("org.rnqnx.devtools", "org.rnqnx.devtools");
  key("org.dev", "org.dev");
  key("Window 123", "");
  key("", "");
  char a[128], b[128], c[128];
  rnqnx_app_key("org.rnqnx.showcase", a, sizeof a);
  rnqnx_app_key("org.rnqnx.showcase.dev", b, sizeof b);
  rnqnx_app_key("org.rnqnx.animshowcase", c, sizeof c);
  assert(rnqnx_same_app(a, b));
  assert(!rnqnx_same_app(a, c));
  assert(!rnqnx_same_app("", ""));
  puts("appkey: ok");
  return 0;
}
