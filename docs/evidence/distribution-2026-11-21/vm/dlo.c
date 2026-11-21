#include <dlfcn.h>
#include <stdio.h>
int main(int argc, char **argv) {
  int bad = 0;
  for (int i = 1; i < argc; i++) {
    void *h = dlopen(argv[i], RTLD_NOW | RTLD_GLOBAL);
    if (h) { printf("ok %s\n", argv[i]); }
    else { printf("FAIL %s: %s\n", argv[i], dlerror()); bad++; }
  }
  printf("files=%d failed=%d\n", argc - 1, bad);
  return bad != 0;
}
