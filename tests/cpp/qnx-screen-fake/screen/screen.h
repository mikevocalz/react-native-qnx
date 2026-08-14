#ifndef RNQNX_TESTS_QNX_SCREEN_FAKE_SCREEN_SCREEN_H_
#define RNQNX_TESTS_QNX_SCREEN_FAKE_SCREEN_SCREEN_H_

/*
 * Test-only subset of the QNX OS 8 Screen API. This file intentionally mirrors
 * the documented Screen symbol names, numeric constants, opaque handle types,
 * and C function signatures used by QnxScreenSurface; it is not a production
 * replacement for the QNX SDP's <screen/screen.h>.
 *
 * References:
 * https://www.qnx.com/developers/docs/8.0/com.qnx.doc.screen/topic/screen_create_context.html
 * https://www.qnx.com/developers/docs/8.0/com.qnx.doc.screen/topic/screen_8h_1Screen_Property_Types.html
 * https://www.qnx.com/developers/docs/8.0/com.qnx.doc.screen/topic/group__screen__blits_1Screen_Blit_Types.html
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _screen_context* screen_context_t;
typedef struct _screen_window* screen_window_t;
typedef struct _screen_buffer* screen_buffer_t;

enum {
  SCREEN_APPLICATION_CONTEXT = 0,
};

enum {
  SCREEN_APPLICATION_WINDOW = 0,
  SCREEN_CHILD_WINDOW = 1,
  SCREEN_EMBEDDED_WINDOW = 2,
  SCREEN_ROOT_WINDOW = 4,
};

enum {
  SCREEN_FORMAT_RGBA8888 = 8,
};

enum {
  SCREEN_USAGE_NATIVE = (1 << 3),
};

enum {
  SCREEN_PROPERTY_BUFFER_SIZE = 5,
  SCREEN_PROPERTY_FORMAT = 14,
  SCREEN_PROPERTY_RENDER_BUFFERS = 37,
  SCREEN_PROPERTY_SIZE = 40,
  SCREEN_PROPERTY_USAGE = 48,
  SCREEN_PROPERTY_RENDER_BUFFER_COUNT = 53,
};

enum {
  SCREEN_BLIT_END = 0,
  SCREEN_BLIT_DESTINATION_X = 5,
  SCREEN_BLIT_DESTINATION_Y = 6,
  SCREEN_BLIT_DESTINATION_WIDTH = 7,
  SCREEN_BLIT_DESTINATION_HEIGHT = 8,
  SCREEN_BLIT_COLOR = 12,
};

int screen_create_context(screen_context_t* pctx, int flags);
int screen_destroy_context(screen_context_t ctx);

int screen_create_window_type(screen_window_t* pwin, screen_context_t ctx, int type);
int screen_destroy_window(screen_window_t win);

int screen_set_window_property_iv(screen_window_t win, int pname, const int* param);
int screen_get_window_property_iv(screen_window_t win, int pname, int* param);
int screen_get_window_property_pv(screen_window_t win, int pname, void** param);

int screen_create_window_buffers(screen_window_t win, int count);

int screen_fill(screen_context_t ctx, screen_buffer_t dst, const int* attribs);
int screen_post_window(
    screen_window_t win,
    screen_buffer_t buffer,
    int count,
    const int* dirty_rects,
    int flags);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // RNQNX_TESTS_QNX_SCREEN_FAKE_SCREEN_SCREEN_H_
