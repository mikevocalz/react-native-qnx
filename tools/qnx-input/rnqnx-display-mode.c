/* rnqnx-display-mode: print the current mode of each QNX Screen display, for
 * performance receipts that need the refresh rate. Read only: it creates a
 * Screen context, reads SCREEN_PROPERTY_MODE and SCREEN_PROPERTY_SIZE, and
 * exits. It does not open the DRM device and does not touch the display.
 * Output, one line per display:
 *   RNQNX_DISPLAY index=0 size=1280x720 mode=1280x720@60 interlaced=0 aspect=16:9 */
#include <screen/screen.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
  screen_context_t ctx;
  if (screen_create_context(&ctx, SCREEN_APPLICATION_CONTEXT)) {
    perror("screen_create_context");
    return 1;
  }
  int count = 0;
  if (screen_get_context_property_iv(ctx, SCREEN_PROPERTY_DISPLAY_COUNT, &count) || count < 1) {
    fprintf(stderr, "no Screen display\n");
    screen_destroy_context(ctx);
    return 1;
  }
  screen_display_t* displays = calloc((size_t)count, sizeof(*displays));
  if (!displays || screen_get_context_property_pv(ctx, SCREEN_PROPERTY_DISPLAYS, (void**)displays)) {
    fprintf(stderr, "cannot list displays\n");
    free(displays);
    screen_destroy_context(ctx);
    return 1;
  }
  int rc = 0;
  for (int i = 0; i < count; ++i) {
    int size[2] = {0, 0};
    screen_display_mode_t mode;
    screen_get_display_property_iv(displays[i], SCREEN_PROPERTY_SIZE, size);
    if (screen_get_display_property_pv(displays[i], SCREEN_PROPERTY_MODE, (void**)&mode)) {
      perror("SCREEN_PROPERTY_MODE");
      rc = 1;
      continue;
    }
    printf("RNQNX_DISPLAY index=%d size=%dx%d mode=%ux%u@%u interlaced=%u aspect=%u:%u\n", i, size[0],
           size[1], mode.width, mode.height, mode.refresh, mode.interlaced, mode.aspect_ratio[0],
           mode.aspect_ratio[1]);
  }
  free(displays);
  screen_destroy_context(ctx);
  return rc;
}
