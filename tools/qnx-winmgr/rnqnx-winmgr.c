/*
 * rnqnx-winmgr: a small stacking window manager for QNX Screen.
 *
 * Copyright (c) 2026 react-native-qnx contributors. MIT License, see LICENSE
 * at the repository root. Written from the public Screen API
 * (screen/screen.h); it contains no code from QNX's fullscreen-winmgr, whose
 * source is not published.
 *
 * It speaks the launcher protocol of the QNX 8.0 Quick Start desktop
 * (demolauncher), so the stock launcher keeps working unchanged:
 *   launcher -> manager  "can_close=N=Launcher=Y", "Tag=<tag>", "switch=<tag>"
 *   manager  -> launcher "closed=<tag>"
 * carried as SCREEN_EVENT_MANAGER events with SCREEN_PROPERTY_USER_DATA.
 *
 * Unlike fullscreen-winmgr it keeps the launcher on screen as the desktop and
 * leaves every other window at the size and position its app asks for:
 *   - the launcher window fills the display at the bottom of the stack;
 *   - app windows stack above it in the order they were last raised;
 *   - a dock along the bottom edge lists the running app windows; a click on
 *     an entry restores a minimized window, raises a covered one, or
 *     minimizes the one in front;
 *   - every app window gets SCREEN_PROPERTY_MANAGER_STRING "rnqnx-winmgr
 *     windowed", so an app can tell it may use a real window of its own size;
 *   - Alt+Tab opens a switcher with each app's icon and name; Tab again moves
 *     the selection, Delete closes the selected app, Escape cancels, and
 *     releasing Alt brings the selected window to the front (restoring it
 *     when minimized). WINMGR_KEYMOD and WINMGR_HOTKEY change the keys as
 *     they do for fullscreen-winmgr.
 * Apps send requests the way the launcher does, as manager messages naming
 * their window:
 *   "rnqnx=minimize"  hide the window (it stays in the dock)
 *   "rnqnx=restore"   show and raise it
 *   "rnqnx=raise"     raise it
 *
 * Usage: rnqnx-winmgr [-v] [-s list] [-d] [-m] [-l n]
 *   -v  log every event to stderr. -s, -d, -m and -l are accepted for
 *   command-line compatibility with fullscreen-winmgr and ignored.
 * Environment:
 *   WINMGR_KEYMOD  shift, ctrl, alt (default) or ralt
 *   WINMGR_HOTKEY  tab (default), space, grave, f1-f12, q, w, t or m
 */

#include <screen/screen.h>

#include "appkey.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include <png.h>

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/keycodes.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define MAX_WINDOWS 64
#define MANAGER_REPLY "rnqnx-winmgr windowed"
#define DESKTOP_DIR "/system/etc/desktop_files"
#define DOCK_HEIGHT 56
#define DOCK_MARGIN 8
/* The launcher draws a status banner along the bottom edge (IP address,
 * hints); the dock floats just above it. */
#define LAUNCHER_BANNER 60
#define DOCK_PAD 6
#define ITEM_ICON 36
#define ITEM_MAX_TEXT 200
#define DOCK_Z 1000
#define APP_Z_BASE 10
/* Alt+Tab switcher: a panel of tiles centred on the display. */
#define SWITCHER_Z 2000
#define TILE_W 168
#define TILE_H 128
#define TILE_ICON 64
#define TILE_GAP 10
#define PANEL_PAD 18
#define PANEL_CAPTION 34
/* Seconds an app gets to close its window after the switcher asks before it
 * is sent SIGTERM. */
#define CLOSE_GRACE_S 3

typedef struct {
  screen_window_t win;
  pid_t pid;
  bool launcher;
  bool posted;
  bool minimized;
  bool hidden_by_app; /* the app itself set VISIBLE=0 */
  char tag[128];
  char app[128]; /* app key (appkey.h): build variants of one app share it */
  bool duplicate_warned;
  unsigned post_seq; /* order the window was posted in; the app's first window is the lowest */
  char title[128];
  uint32_t* icon; /* ITEM_ICON x ITEM_ICON premultiplied BGRA, or NULL */
  uint32_t* tile_icon; /* TILE_ICON x TILE_ICON, for the switcher */
  int dock_x, dock_w; /* hit area in the dock, display coordinates */
} managed_t;

static screen_context_t ctx;
static screen_display_t display;
static int display_size[2];
static bool verbose;

static managed_t windows[MAX_WINDOWS];
static int window_count;
/* App windows from bottom to top; indexes into windows[] are unstable, so the
 * stack holds window handles. */
static screen_window_t stack[MAX_WINDOWS];
static int stack_count;

static unsigned post_counter;
static char pending_tag[128];
static time_t pending_tag_time;

static screen_window_t dock;
static int dock_rect[4]; /* x, y, w, h on the display */
static FT_Library ft;
static FT_Face face;

/* Alt+Tab. The keyboard session takes every key press first; keys that are
 * not the switcher's go on to the focused window. */
static screen_session_t key_session;
static int switch_mod = KEYMOD_ALT;
static int switch_mod_keys[2] = {KEYCODE_LEFT_ALT, KEYCODE_RIGHT_ALT};
static int switch_key = KEYCODE_TAB;
static screen_window_t switcher;
static bool switcher_open;
static screen_window_t switch_list[MAX_WINDOWS]; /* most recently used first */
static int switch_count;
static int switch_sel;
static int switcher_rect[4];
/* Keys whose press the switcher used; their releases are not passed on
 * either, so an app never sees a release without its press. */
static int swallowed[8];
static int swallowed_count;
static int tile_x[MAX_WINDOWS], tile_y[MAX_WINDOWS]; /* display coordinates */

/* Windows asked to close from the switcher, with their SIGTERM deadline. */
typedef struct {
  screen_window_t win;
  pid_t pid;
  struct timespec deadline;
} closing_t;
static closing_t closing[MAX_WINDOWS];
static int closing_count;

#define LOG(...) \
  do { \
    if (verbose) { fprintf(stderr, "rnqnx-winmgr: " __VA_ARGS__); fputc('\n', stderr); } \
  } while (0)
#define WARN(...) \
  do { fprintf(stderr, "rnqnx-winmgr: " __VA_ARGS__); fputc('\n', stderr); } while (0)

/* ---------- bookkeeping ---------- */

static managed_t* find(screen_window_t win) {
  for (int i = 0; i < window_count; ++i)
    if (windows[i].win == win) return &windows[i];
  return NULL;
}

static managed_t* find_tag(const char* tag) {
  for (int i = 0; i < window_count; ++i)
    if (windows[i].tag[0] && strcmp(windows[i].tag, tag) == 0) return &windows[i];
  return NULL;
}

static managed_t* launcher_window(void) {
  for (int i = 0; i < window_count; ++i)
    if (windows[i].launcher) return &windows[i];
  return NULL;
}

static void stack_remove(screen_window_t win) {
  for (int i = 0; i < stack_count; ++i) {
    if (stack[i] != win) continue;
    memmove(&stack[i], &stack[i + 1], (size_t)(stack_count - i - 1) * sizeof stack[0]);
    --stack_count;
    return;
  }
}

static void stack_push_top(screen_window_t win) {
  stack_remove(win);
  if (stack_count < MAX_WINDOWS) stack[stack_count++] = win;
}

/* Topmost app window the user can see, or NULL. */
static managed_t* front_window(void) {
  for (int i = stack_count - 1; i >= 0; --i) {
    managed_t* m = find(stack[i]);
    if (m && m->posted && !m->minimized && !m->hidden_by_app) return m;
  }
  return NULL;
}

/* ---------- desktop entries: title and icon for a launcher tag ---------- */

static void trim(char* s) {
  size_t n = strlen(s);
  while (n && isspace((unsigned char)s[n - 1])) s[--n] = 0;
}

/* Reads Name= and Icon= from the desktop entry whose file name or Name=
 * matches `tag`. Returns false when none does. */
static bool desktop_entry_for(const char* tag, char* name, size_t name_len, char* icon,
                              size_t icon_len) {
  char path[512];
  const char* candidates[3];
  char with_ext[256];
  snprintf(with_ext, sizeof with_ext, "%s.desktop", tag);
  candidates[0] = tag;
  candidates[1] = with_ext;
  candidates[2] = NULL;
  for (int c = 0; candidates[c]; ++c) {
    if (strchr(candidates[c], '/')) continue;
    snprintf(path, sizeof path, "%s/%s", DESKTOP_DIR, candidates[c]);
    FILE* f = fopen(path, "r");
    if (!f) continue;
    char line[512];
    name[0] = icon[0] = 0;
    while (fgets(line, sizeof line, f)) {
      trim(line);
      if (!strncmp(line, "Name=", 5)) snprintf(name, name_len, "%s", line + 5);
      if (!strncmp(line, "Icon=", 5)) snprintf(icon, icon_len, "%s", line + 5);
    }
    fclose(f);
    return true;
  }
  return false;
}

/* Name of the process that owns the window, from /proc. */
static void process_name(pid_t pid, char* out, size_t len) {
  char path[64];
  snprintf(path, sizeof path, "/proc/%d/exefile", (int)pid);
  FILE* f = fopen(path, "r");
  out[0] = 0;
  if (f) {
    char buf[512] = {0};
    if (fgets(buf, sizeof buf, f)) {
      trim(buf);
      const char* base = strrchr(buf, '/');
      snprintf(out, len, "%s", base ? base + 1 : buf);
    }
    fclose(f);
  }
  if (!out[0]) snprintf(out, len, "Window %d", (int)pid);
}

/* Reads a PNG and box-filters it to size x size premultiplied BGRA. */
static uint32_t* load_icon(const char* path, int size) {
  if (!path || !path[0]) return NULL;
  png_image image;
  memset(&image, 0, sizeof image);
  image.version = PNG_IMAGE_VERSION;
  if (!png_image_begin_read_from_file(&image, path)) return NULL;
  image.format = PNG_FORMAT_BGRA;
  size_t stride = PNG_IMAGE_ROW_STRIDE(image);
  uint8_t* src = malloc(PNG_IMAGE_BUFFER_SIZE(image, stride));
  if (!src) {
    png_image_free(&image);
    return NULL;
  }
  if (!png_image_finish_read(&image, NULL, src, (png_int_32)stride, NULL)) {
    free(src);
    return NULL;
  }
  uint32_t* out = calloc((size_t)size * (size_t)size, sizeof *out);
  if (out) {
    for (int y = 0; y < size; ++y) {
      for (int x = 0; x < size; ++x) {
        int x0 = x * (int)image.width / size, x1 = (x + 1) * (int)image.width / size;
        int y0 = y * (int)image.height / size, y1 = (y + 1) * (int)image.height / size;
        if (x1 <= x0) x1 = x0 + 1;
        if (y1 <= y0) y1 = y0 + 1;
        unsigned long b = 0, g = 0, r = 0, a = 0, n = 0;
        for (int sy = y0; sy < y1; ++sy) {
          const uint8_t* row = src + (size_t)sy * stride;
          for (int sx = x0; sx < x1; ++sx) {
            const uint8_t* p = row + sx * 4;
            b += (unsigned long)p[0] * p[3];
            g += (unsigned long)p[1] * p[3];
            r += (unsigned long)p[2] * p[3];
            a += p[3];
            ++n;
          }
        }
        out[y * size + x] = (uint32_t)((a / n) << 24 | (r / n / 255) << 16 |
                                            (g / n / 255) << 8 | (b / n / 255));
      }
    }
  }
  free(src);
  return out;
}

static void describe_window(managed_t* m) {
  char name[128] = {0}, icon[256] = {0};
  char process[128];
  process_name(m->pid, process, sizeof process);
  rnqnx_app_key(process, m->app, sizeof m->app);
  /* The launcher's tag names the entry it started; otherwise a desktop entry
   * named after the program (org.example.app.desktop) still gives the app's
   * own name and icon. */
  if ((m->tag[0] && desktop_entry_for(m->tag, name, sizeof name, icon, sizeof icon) && name[0]) ||
      (desktop_entry_for(process, name, sizeof name, icon, sizeof icon) && name[0])) {
    snprintf(m->title, sizeof m->title, "%s", name);
  } else {
    snprintf(m->title, sizeof m->title, "%s", process);
  }
  free(m->icon);
  free(m->tile_icon);
  m->icon = load_icon(icon, ITEM_ICON);
  m->tile_icon = load_icon(icon, TILE_ICON);
}

/* ---------- dock drawing ---------- */

static int text_width(const char* s) {
  if (!face) return 0;
  int w = 0;
  for (const unsigned char* p = (const unsigned char*)s; *p; ++p) {
    if (FT_Load_Char(face, *p, FT_LOAD_DEFAULT)) continue;
    w += (int)(face->glyph->advance.x >> 6);
  }
  return w;
}

static inline uint32_t blend(uint32_t dst, uint32_t src) {
  uint32_t sa = src >> 24;
  if (sa == 255) return src;
  if (sa == 0) return dst;
  uint32_t out = 0;
  for (int shift = 0; shift < 32; shift += 8) {
    uint32_t s = (src >> shift) & 0xff, d = (dst >> shift) & 0xff;
    out |= ((s + d * (255 - sa) / 255) & 0xff) << shift;
  }
  return out;
}

static uint32_t premul(uint32_t argb) {
  uint32_t a = argb >> 24;
  uint32_t r = ((argb >> 16) & 0xff) * a / 255, g = ((argb >> 8) & 0xff) * a / 255,
           b = (argb & 0xff) * a / 255;
  return a << 24 | r << 16 | g << 8 | b;
}

static void fill_round_rect(uint32_t* px, int stride, int bw, int bh, int x, int y, int w, int h,
                            int radius, uint32_t argb) {
  uint32_t color = premul(argb);
  for (int j = 0; j < h; ++j) {
    int yy = y + j;
    if (yy < 0 || yy >= bh) continue;
    for (int i = 0; i < w; ++i) {
      int xx = x + i;
      if (xx < 0 || xx >= bw) continue;
      int dx = i < radius ? radius - i - 1 : (i >= w - radius ? i - (w - radius) : -1);
      int dy = j < radius ? radius - j - 1 : (j >= h - radius ? j - (h - radius) : -1);
      if (dx >= 0 && dy >= 0 && dx * dx + dy * dy > radius * radius) continue;
      px[yy * stride + xx] = blend(px[yy * stride + xx], color);
    }
  }
}

static void draw_text(uint32_t* px, int stride, int bw, int bh, int x, int baseline,
                      const char* s, int max_w, uint32_t rgb) {
  if (!face) return;
  int pen = x;
  for (const unsigned char* p = (const unsigned char*)s; *p; ++p) {
    if (FT_Load_Char(face, *p, FT_LOAD_RENDER)) continue;
    FT_GlyphSlot g = face->glyph;
    int adv = (int)(g->advance.x >> 6);
    if (pen + adv > x + max_w) break;
    for (unsigned r = 0; r < g->bitmap.rows; ++r) {
      int yy = baseline - g->bitmap_top + (int)r;
      if (yy < 0 || yy >= bh) continue;
      for (unsigned c = 0; c < g->bitmap.width; ++c) {
        int xx = pen + g->bitmap_left + (int)c;
        if (xx < 0 || xx >= bw) continue;
        uint32_t a = g->bitmap.buffer[r * (unsigned)g->bitmap.pitch + c];
        px[yy * stride + xx] = blend(px[yy * stride + xx], premul(a << 24 | rgb));
      }
    }
    pen += adv;
  }
}

/* A coloured square with the title's first letter, for an app without an
 * icon. */
static void draw_monogram(uint32_t* px, int stride, int bw, int bh, int x, int y, int size,
                          const char* title) {
  fill_round_rect(px, stride, bw, bh, x, y, size, size, size / 4, 0xff3a6ea5);
  char letter[2] = {(char)toupper((unsigned char)(title[0] ? title[0] : '?')), 0};
  if (face) FT_Set_Pixel_Sizes(face, 0, (FT_UInt)(size / 2));
  int tw = text_width(letter);
  draw_text(px, stride, bw, bh, x + (size - tw) / 2, y + size / 2 + size / 6, letter, size,
            0xffffff);
  if (face) FT_Set_Pixel_Sizes(face, 0, 17);
}

/* Copies a premultiplied size x size icon, at half opacity when dim. */
static void draw_icon(uint32_t* px, int stride, int x, int y, const uint32_t* icon, int size,
                      bool dim) {
  for (int yy = 0; yy < size; ++yy)
    for (int xx = 0; xx < size; ++xx) {
      uint32_t* d = &px[(y + yy) * stride + x + xx];
      uint32_t s = icon[yy * size + xx];
      /* Premultiplied, so halving every channel halves the opacity. */
      if (dim) s = s >> 1 & 0x7f7f7f7f;
      *d = blend(*d, s);
    }
}

/* The first-posted app window of the same app as `m`, or NULL when `m` is it.
 * One app never has two entries or two windows in front of the user. */
static managed_t* first_window_of_app(const managed_t* m) {
  if (!m->app[0]) return NULL;
  managed_t* first = NULL;
  for (int i = 0; i < window_count; ++i) {
    managed_t* other = &windows[i];
    if (other->launcher || !other->posted || other->hidden_by_app) continue;
    if (!rnqnx_same_app(other->app, m->app)) continue;
    if (!first || other->post_seq < first->post_seq) first = other;
  }
  return first == m ? NULL : first;
}

/* Entries shown in the dock: posted app windows the app has not hidden, one
 * per app. Windows that share an app key collapse into the first. */
static int dock_entries(managed_t** out) {
  int n = 0;
  for (int i = 0; i < window_count; ++i) {
    managed_t* m = &windows[i];
    if (m->launcher || !m->posted || m->hidden_by_app) continue;
    if (first_window_of_app(m)) continue;
    out[n++] = m;
  }
  return n;
}

/* True when the dock entry `m` stands for the front window (its own or a
 * duplicate of the same app). */
static bool entry_is_front(const managed_t* m, const managed_t* front) {
  return front && (m == front || rnqnx_same_app(m->app, front->app));
}

static void update_dock(void) {
  managed_t* entries[MAX_WINDOWS];
  int n = dock_entries(entries);
  int visible = n > 0;
  if (!dock) return;
  if (!visible) {
    screen_set_window_property_iv(dock, SCREEN_PROPERTY_VISIBLE, &visible);
    return;
  }
  managed_t* front = front_window();

  int width = DOCK_PAD;
  int item_w[MAX_WINDOWS];
  for (int i = 0; i < n; ++i) {
    int tw = text_width(entries[i]->title);
    if (tw > ITEM_MAX_TEXT) tw = ITEM_MAX_TEXT;
    item_w[i] = 8 + ITEM_ICON + 10 + tw + 14;
    width += item_w[i] + DOCK_PAD;
  }
  const int max_width = display_size[0] - 2 * DOCK_MARGIN;
  if (width > max_width && n > 0) {
    /* Too many entries: share the width so every window keeps a clickable
     * entry; titles are cut and, below the icon's width, left out. */
    int share = (max_width - DOCK_PAD) / n - DOCK_PAD;
    if (share < ITEM_ICON + 16) share = ITEM_ICON + 16;
    width = DOCK_PAD;
    for (int i = 0; i < n; ++i) {
      if (item_w[i] > share) item_w[i] = share;
      width += item_w[i] + DOCK_PAD;
    }
  }
  if (width > max_width) width = max_width;
  int height = DOCK_HEIGHT;
  dock_rect[0] = (display_size[0] - width) / 2;
  dock_rect[1] = display_size[1] - LAUNCHER_BANNER - height - DOCK_MARGIN;
  dock_rect[2] = width;
  dock_rect[3] = height;

  /* Buffers are made once at the largest size the dock can have. The
   * property fills one handle per buffer; the first is the one to draw. */
  screen_buffer_t buffers[2] = {NULL, NULL};
  if (screen_get_window_property_pv(dock, SCREEN_PROPERTY_RENDER_BUFFERS, (void**)buffers) ||
      !buffers[0])
    return;
  screen_buffer_t buffer = buffers[0];
  uint32_t* px = NULL;
  int stride_bytes = 0;
  screen_get_buffer_property_pv(buffer, SCREEN_PROPERTY_POINTER, (void**)&px);
  screen_get_buffer_property_iv(buffer, SCREEN_PROPERTY_STRIDE, &stride_bytes);
  if (!px) return;
  int stride = stride_bytes / 4;
  for (int y = 0; y < height; ++y) memset(px + y * stride, 0, (size_t)width * 4);

  fill_round_rect(px, stride, width, height, 0, 0, width, height, 14, 0xe6141b24);
  int x = DOCK_PAD;
  for (int i = 0; i < n; ++i) {
    managed_t* m = entries[i];
    int w = item_w[i];
    if (x + w > width) w = width - x - DOCK_PAD;
    if (w <= 0) {
      m->dock_x = m->dock_w = 0;
      continue;
    }
    uint32_t bg = entry_is_front(m, front) ? 0xff2f4258 : 0x00000000;
    if (bg) fill_round_rect(px, stride, width, height, x, DOCK_PAD, w, height - 2 * DOCK_PAD, 10, bg);
    int iy = (height - ITEM_ICON) / 2;
    if (w < 8 + ITEM_ICON + 8) {
      /* No room left for the icon in a full dock. */
    } else if (m->icon) {
      /* Half opacity for a minimized window. */
      draw_icon(px, stride, x + 8, iy, m->icon, ITEM_ICON, m->minimized);
    } else {
      draw_monogram(px, stride, width, height, x + 8, iy, ITEM_ICON, m->title);
    }
    if (w > 8 + ITEM_ICON + 18)
      draw_text(px, stride, width, height, x + 8 + ITEM_ICON + 10, height / 2 + 6, m->title,
              w - (8 + ITEM_ICON + 10) - 8, m->minimized ? 0x8a96a3 : 0xf2f5f8);
    if (entry_is_front(m, front)) fill_round_rect(px, stride, width, height, x + w / 2 - 10, height - 5, 20, 3, 1,
                                    0xff4fc3f7);
    m->dock_x = dock_rect[0] + x;
    m->dock_w = w;
    x += w + DOCK_PAD;
  }

  int pos[2] = {dock_rect[0], dock_rect[1]};
  int size[2] = {width, height};
  int src[2] = {width, height};
  screen_set_window_property_iv(dock, SCREEN_PROPERTY_POSITION, pos);
  screen_set_window_property_iv(dock, SCREEN_PROPERTY_SIZE, size);
  screen_set_window_property_iv(dock, SCREEN_PROPERTY_SOURCE_SIZE, src);
  screen_set_window_property_iv(dock, SCREEN_PROPERTY_VISIBLE, &visible);
  int rect[4] = {0, 0, width, height};
  screen_post_window(dock, buffer, 1, rect, 0);
}

static void create_dock(void) {
  if (screen_create_window(&dock, ctx)) {
    WARN("cannot create the dock window: %s", strerror(errno));
    dock = NULL;
    return;
  }
  const char* id = "rnqnx-winmgr-dock";
  screen_set_window_property_cv(dock, SCREEN_PROPERTY_ID_STRING, (int)strlen(id), id);
  int usage = SCREEN_USAGE_WRITE | SCREEN_USAGE_NATIVE;
  int format = SCREEN_FORMAT_RGBA8888;
  int transparency = SCREEN_TRANSPARENCY_SOURCE_OVER;
  int premultiplied = SCREEN_PRE_MULTIPLIED_ALPHA;
  int z = DOCK_Z;
  int visible = 0;
  int buffer_size[2] = {display_size[0], DOCK_HEIGHT};
  screen_set_window_property_iv(dock, SCREEN_PROPERTY_USAGE, &usage);
  screen_set_window_property_iv(dock, SCREEN_PROPERTY_FORMAT, &format);
  screen_set_window_property_iv(dock, SCREEN_PROPERTY_TRANSPARENCY, &transparency);
  screen_set_window_property_iv(dock, SCREEN_PROPERTY_ALPHA_MODE, &premultiplied);
  screen_set_window_property_iv(dock, SCREEN_PROPERTY_ZORDER, &z);
  int sensitivity = SCREEN_SENSITIVITY_MASK_NO_FOCUS;
  screen_set_window_property_iv(dock, SCREEN_PROPERTY_SENSITIVITY, &sensitivity);
  screen_set_window_property_iv(dock, SCREEN_PROPERTY_VISIBLE, &visible);
  screen_set_window_property_iv(dock, SCREEN_PROPERTY_BUFFER_SIZE, buffer_size);
  if (screen_create_window_buffers(dock, 2)) {
    WARN("cannot create the dock buffers: %s", strerror(errno));
    screen_destroy_window(dock);
    dock = NULL;
  }
}

static void load_font(void) {
  static const char* fonts[] = {
      "/usr/share/fonts/dejavu/DejaVuSans.ttf",
      "/usr/share/fonts/DejaVuSans.ttf",
      "/usr/share/fonts/Roboto-Regular.ttf",
      NULL,
  };
  if (FT_Init_FreeType(&ft)) return;
  for (int i = 0; fonts[i]; ++i) {
    if (FT_New_Face(ft, fonts[i], 0, &face) == 0) {
      FT_Set_Pixel_Sizes(face, 0, 17);
      return;
    }
  }
  face = NULL;
  WARN("no dock font found; titles are not drawn");
}

/* ---------- stacking and visibility ---------- */

static void apply_stack(void) {
  managed_t* launcher = launcher_window();
  if (launcher && launcher->posted) {
    int z = 0, visible = 1;
    screen_set_window_property_iv(launcher->win, SCREEN_PROPERTY_ZORDER, &z);
    screen_set_window_property_iv(launcher->win, SCREEN_PROPERTY_VISIBLE, &visible);
  }
  for (int i = 0; i < stack_count; ++i) {
    managed_t* m = find(stack[i]);
    if (!m || !m->posted) continue;
    int z = APP_Z_BASE + i;
    int visible = !m->minimized && !m->hidden_by_app;
    screen_set_window_property_iv(m->win, SCREEN_PROPERTY_ZORDER, &z);
    if (!m->hidden_by_app) screen_set_window_property_iv(m->win, SCREEN_PROPERTY_VISIBLE, &visible);
  }
  managed_t* front = front_window();
  screen_window_t focus = front ? front->win : (launcher ? launcher->win : NULL);
  if (focus) screen_set_display_property_pv(display, SCREEN_PROPERTY_FOCUS, (void**)&focus);
  update_dock();
  screen_flush_context(ctx, 0);
}

static void raise_window(managed_t* m) {
  if (m->launcher) return;
  m->minimized = false;
  stack_push_top(m->win);
  LOG("raise pid=%d '%s'", (int)m->pid, m->title);
  apply_stack();
}

static void minimize_window(managed_t* m) {
  if (m->launcher) return;
  m->minimized = true;
  /* A minimized window goes to the bottom of the stack, so the stack stays in
   * the order Alt+Tab lists: visible windows front to back, then minimized
   * ones. */
  stack_remove(m->win);
  if (stack_count < MAX_WINDOWS) {
    memmove(&stack[1], &stack[0], (size_t)stack_count * sizeof stack[0]);
    stack[0] = m->win;
    ++stack_count;
  }
  LOG("minimize pid=%d '%s'", (int)m->pid, m->title);
  apply_stack();
}

/* ---------- launcher protocol ---------- */

static void send_manager_message(screen_window_t target, const char* key, const char* value) {
  char text[256];
  snprintf(text, sizeof text, "%s=%s", key, value);
  screen_event_t ev;
  if (screen_create_event(&ev)) return;
  int type = SCREEN_EVENT_MANAGER;
  screen_set_event_property_iv(ev, SCREEN_PROPERTY_TYPE, &type);
  screen_set_event_property_pv(ev, SCREEN_PROPERTY_WINDOW, (void**)&target);
  screen_set_event_property_cv(ev, SCREEN_PROPERTY_USER_DATA, (int)strlen(text), text);
  screen_display_t target_display = NULL;
  screen_get_window_property_pv(target, SCREEN_PROPERTY_DISPLAY, (void**)&target_display);
  if (screen_inject_event(target_display ? target_display : display, ev))
    WARN("cannot send '%s': %s", text, strerror(errno));
  screen_destroy_event(ev);
}

static void handle_manager_message(screen_window_t from, const char* data) {
  LOG("message '%s'", data);
  char copy[512];
  snprintf(copy, sizeof copy, "%s", data);
  managed_t* m = from ? find(from) : NULL;
  /* "key=value=key=value": split on '=' by hand so an empty value keeps the
   * pairs aligned. */
  char* next = copy;
  while (next && *next) {
    char* key = next;
    char* value = strchr(key, '=');
    if (!value) break;
    *value++ = 0;
    next = strchr(value, '=');
    if (next) *next++ = 0;
    if (!strcasecmp(key, "Launcher")) {
      if ((value[0] == 'Y' || value[0] == 'y') && m && !launcher_window()) {
        m->launcher = true;
        stack_remove(m->win);
        int self_layout = 0;
        screen_set_window_property_iv(m->win, SCREEN_PROPERTY_SELF_LAYOUT, &self_layout);
        if (m->posted) {
          int size[2] = {display_size[0], display_size[1]};
          int pos[2] = {0, 0};
          screen_set_window_property_iv(m->win, SCREEN_PROPERTY_SIZE, size);
          screen_set_window_property_iv(m->win, SCREEN_PROPERTY_POSITION, pos);
        }
        apply_stack();
      }
    } else if (!strcasecmp(key, "Tag")) {
      snprintf(pending_tag, sizeof pending_tag, "%s", value);
      pending_tag_time = time(NULL);
    } else if (!strcasecmp(key, "switch")) {
      managed_t* target = find_tag(value);
      if (target) raise_window(target);
    } else if (!strcasecmp(key, "rnqnx") && m && !m->launcher) {
      /* Requests from an app's own window controls. */
      if (!strcasecmp(value, "minimize")) minimize_window(m);
      else if (!strcasecmp(value, "restore") || !strcasecmp(value, "raise")) raise_window(m);
    }
    /* can_close, Frame and Title need nothing here. */
  }
}

/* ---------- Alt+Tab switcher ---------- */

static void create_switcher(void) {
  if (screen_create_window(&switcher, ctx)) {
    WARN("cannot create the switcher window: %s", strerror(errno));
    switcher = NULL;
    return;
  }
  const char* id = "rnqnx-winmgr-switcher";
  screen_set_window_property_cv(switcher, SCREEN_PROPERTY_ID_STRING, (int)strlen(id), id);
  int usage = SCREEN_USAGE_WRITE | SCREEN_USAGE_NATIVE;
  int format = SCREEN_FORMAT_RGBA8888;
  int transparency = SCREEN_TRANSPARENCY_SOURCE_OVER;
  int premultiplied = SCREEN_PRE_MULTIPLIED_ALPHA;
  int z = SWITCHER_Z;
  int visible = 0;
  int buffer_size[2] = {display_size[0], display_size[1]};
  screen_set_window_property_iv(switcher, SCREEN_PROPERTY_USAGE, &usage);
  screen_set_window_property_iv(switcher, SCREEN_PROPERTY_FORMAT, &format);
  screen_set_window_property_iv(switcher, SCREEN_PROPERTY_TRANSPARENCY, &transparency);
  screen_set_window_property_iv(switcher, SCREEN_PROPERTY_ALPHA_MODE, &premultiplied);
  screen_set_window_property_iv(switcher, SCREEN_PROPERTY_ZORDER, &z);
  /* Clicks reach it, keyboard focus stays with the app. */
  int sensitivity = SCREEN_SENSITIVITY_MASK_NO_FOCUS;
  screen_set_window_property_iv(switcher, SCREEN_PROPERTY_SENSITIVITY, &sensitivity);
  screen_set_window_property_iv(switcher, SCREEN_PROPERTY_VISIBLE, &visible);
  screen_set_window_property_iv(switcher, SCREEN_PROPERTY_BUFFER_SIZE, buffer_size);
  if (screen_create_window_buffers(switcher, 2)) {
    WARN("cannot create the switcher buffers: %s", strerror(errno));
    screen_destroy_window(switcher);
    switcher = NULL;
  }
}

/* Copies s into out, cut with "..." when it is wider than max_w. */
static void fit_text(const char* s, int max_w, char* out, size_t len) {
  snprintf(out, len, "%s", s);
  if (text_width(out) <= max_w) return;
  for (size_t n = strlen(out); n > 0; --n) {
    snprintf(out, len, "%.*s...", (int)n - 1, s);
    if (text_width(out) <= max_w) return;
  }
}

static void draw_switcher(void) {
  if (!switcher || !switcher_open || switch_count == 0) return;
  const int margin = 40;
  int cols = (display_size[0] - 2 * margin - 2 * PANEL_PAD + TILE_GAP) / (TILE_W + TILE_GAP);
  if (cols < 1) cols = 1;
  if (cols > switch_count) cols = switch_count;
  int rows = (switch_count + cols - 1) / cols;
  int max_rows = (display_size[1] - 2 * margin - 2 * PANEL_PAD - PANEL_CAPTION + TILE_GAP) /
                 (TILE_H + TILE_GAP);
  if (max_rows < 1) max_rows = 1;
  if (rows > max_rows) rows = max_rows; /* more windows than fit: the rest are not drawn */
  int width = 2 * PANEL_PAD + cols * TILE_W + (cols - 1) * TILE_GAP;
  int height = 2 * PANEL_PAD + rows * TILE_H + (rows - 1) * TILE_GAP + PANEL_CAPTION;
  /* Wide enough for the caption. */
  const char* caption = "Tab: next   Del: close   Esc: cancel";
  int caption_w = text_width(caption);
  if (width < caption_w + 2 * PANEL_PAD) width = caption_w + 2 * PANEL_PAD;
  if (width > display_size[0]) width = display_size[0];
  if (height > display_size[1]) return; /* a display too small for one row */
  switcher_rect[0] = (display_size[0] - width) / 2;
  switcher_rect[1] = (display_size[1] - height) / 2;
  switcher_rect[2] = width;
  switcher_rect[3] = height;

  screen_buffer_t buffers[2] = {NULL, NULL};
  if (screen_get_window_property_pv(switcher, SCREEN_PROPERTY_RENDER_BUFFERS, (void**)buffers) ||
      !buffers[0])
    return;
  uint32_t* px = NULL;
  int stride_bytes = 0;
  screen_get_buffer_property_pv(buffers[0], SCREEN_PROPERTY_POINTER, (void**)&px);
  screen_get_buffer_property_iv(buffers[0], SCREEN_PROPERTY_STRIDE, &stride_bytes);
  if (!px) return;
  int stride = stride_bytes / 4;
  for (int y = 0; y < height; ++y) memset(px + y * stride, 0, (size_t)width * 4);
  fill_round_rect(px, stride, width, height, 0, 0, width, height, 18, 0xff141b24);

  int grid_w = cols * TILE_W + (cols - 1) * TILE_GAP;
  int x0 = (width - grid_w) / 2;
  for (int i = 0; i < switch_count; ++i) {
    tile_x[i] = tile_y[i] = -1;
    int row = i / cols, col = i % cols;
    if (row >= rows) continue;
    managed_t* m = find(switch_list[i]);
    if (!m) continue;
    int tx = x0 + col * (TILE_W + TILE_GAP);
    int ty = PANEL_PAD + row * (TILE_H + TILE_GAP);
    tile_x[i] = switcher_rect[0] + tx;
    tile_y[i] = switcher_rect[1] + ty;
    if (i == switch_sel) {
      fill_round_rect(px, stride, width, height, tx, ty, TILE_W, TILE_H, 12, 0xff4fc3f7);
      fill_round_rect(px, stride, width, height, tx + 3, ty + 3, TILE_W - 6, TILE_H - 6, 10,
                      0xff2f4258);
    }
    int ix = tx + (TILE_W - TILE_ICON) / 2, iy = ty + 14;
    if (m->tile_icon)
      draw_icon(px, stride, ix, iy, m->tile_icon, TILE_ICON, m->minimized);
    else
      draw_monogram(px, stride, width, height, ix, iy, TILE_ICON, m->title);
    char name[sizeof m->title + 3];
    fit_text(m->title, TILE_W - 16, name, sizeof name);
    int tw = text_width(name);
    draw_text(px, stride, width, height, tx + (TILE_W - tw) / 2, ty + TILE_H - 22, name, TILE_W - 16,
              m->minimized ? 0x8a96a3 : 0xf2f5f8);
  }
  draw_text(px, stride, width, height, (width - caption_w) / 2, height - PANEL_PAD - 4, caption,
            width - 2 * PANEL_PAD, 0x8a96a3);

  int pos[2] = {switcher_rect[0], switcher_rect[1]};
  int size[2] = {width, height};
  int visible = 1;
  screen_set_window_property_iv(switcher, SCREEN_PROPERTY_POSITION, pos);
  screen_set_window_property_iv(switcher, SCREEN_PROPERTY_SIZE, size);
  screen_set_window_property_iv(switcher, SCREEN_PROPERTY_SOURCE_SIZE, size);
  screen_set_window_property_iv(switcher, SCREEN_PROPERTY_VISIBLE, &visible);
  int rect[4] = {0, 0, width, height};
  screen_post_window(switcher, buffers[0], 1, rect, 0);
  screen_flush_context(ctx, 0);
}

static void hide_switcher(void) {
  switcher_open = false;
  switch_count = 0;
  if (!switcher) return;
  int visible = 0;
  screen_set_window_property_iv(switcher, SCREEN_PROPERTY_VISIBLE, &visible);
  screen_flush_context(ctx, 0);
}

/* Opens the switcher with the window after the front one selected, or the
 * last one when going backwards. */
static void open_switcher(bool backwards) {
  switch_count = 0;
  for (int i = stack_count - 1; i >= 0; --i) {
    managed_t* m = find(stack[i]);
    if (m && m->posted && !m->launcher && !m->hidden_by_app) switch_list[switch_count++] = m->win;
  }
  if (switch_count == 0) return;
  managed_t* front = front_window();
  if (backwards)
    switch_sel = switch_count - 1;
  else
    switch_sel = switch_count > 1 && front && front->win == switch_list[0] ? 1 : 0;
  switcher_open = true;
  LOG("switcher open, %d windows, selected %d", switch_count, switch_sel);
  draw_switcher();
}

static void step_switcher(int delta) {
  if (switch_count == 0) return;
  switch_sel = ((switch_sel + delta) % switch_count + switch_count) % switch_count;
  draw_switcher();
}

static bool is_closing(screen_window_t win) {
  for (int i = 0; i < closing_count; ++i)
    if (closing[i].win == win) return true;
  return false;
}

static void activate_switcher(void) {
  managed_t* m = switch_count > 0 ? find(switch_list[switch_sel]) : NULL;
  hide_switcher();
  if (m && is_closing(m->win)) {
    /* Closing it from the switcher: nothing to bring forward. */
    apply_stack();
  } else if (m) {
    LOG("switch to pid=%d '%s'", (int)m->pid, m->title);
    raise_window(m);
  }
}

/* The window left the screen: drop it from the switcher. */
static void switcher_forget(screen_window_t win) {
  for (int i = 0; i < switch_count; ++i) {
    if (switch_list[i] != win) continue;
    memmove(&switch_list[i], &switch_list[i + 1], (size_t)(switch_count - i - 1) * sizeof win);
    --switch_count;
    if (switch_sel > i || switch_sel >= switch_count) --switch_sel;
    if (switch_sel < 0) switch_sel = 0;
    break;
  }
  if (!switcher_open) return;
  if (switch_count == 0) {
    hide_switcher();
    apply_stack();
  } else {
    draw_switcher();
  }
}

/* Asks the app to close the window the way a title-bar close does: a manager
 * close event, which Qt turns into QWindow::close(). An app that is still
 * there CLOSE_GRACE_S seconds later gets SIGTERM. */
static void close_window(managed_t* m) {
  if (m->launcher) return;
  LOG("close request pid=%d '%s'", (int)m->pid, m->title);
  screen_event_t ev;
  bool sent = false;
  if (!screen_create_event(&ev)) {
    int type = SCREEN_EVENT_MANAGER, subtype = SCREEN_EVENT_CLOSE;
    screen_set_event_property_iv(ev, SCREEN_PROPERTY_TYPE, &type);
    screen_set_event_property_pv(ev, SCREEN_PROPERTY_WINDOW, (void**)&m->win);
    screen_set_event_property_iv(ev, SCREEN_PROPERTY_SUBTYPE, &subtype);
    sent = screen_send_event(ctx, ev, m->pid) == 0;
    if (!sent) WARN("cannot send close to pid %d: %s", (int)m->pid, strerror(errno));
    screen_destroy_event(ev);
    screen_flush_context(ctx, 0);
  }
  if (!sent) {
    if (m->pid > 1) kill(m->pid, SIGTERM);
    return;
  }
  for (int i = 0; i < closing_count; ++i)
    if (closing[i].win == m->win) return;
  if (closing_count >= MAX_WINDOWS) return;
  closing_t* c = &closing[closing_count++];
  c->win = m->win;
  c->pid = m->pid;
  clock_gettime(CLOCK_MONOTONIC, &c->deadline);
  c->deadline.tv_sec += CLOSE_GRACE_S;
}

static void closing_forget(screen_window_t win) {
  for (int i = 0; i < closing_count; ++i) {
    if (closing[i].win != win) continue;
    memmove(&closing[i], &closing[i + 1], (size_t)(closing_count - i - 1) * sizeof closing[0]);
    --closing_count;
    return;
  }
}

static int64_t ns_until(const struct timespec* t) {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (int64_t)(t->tv_sec - now.tv_sec) * 1000000000 + (t->tv_nsec - now.tv_nsec);
}

/* SIGTERM for apps that kept a window past their close deadline. Returns the
 * nanoseconds to the next deadline, or -1 when none is left. */
static int64_t check_closing(void) {
  int64_t next = -1;
  for (int i = 0; i < closing_count;) {
    int64_t left = ns_until(&closing[i].deadline);
    if (left > 0) {
      if (next < 0 || left < next) next = left;
      ++i;
      continue;
    }
    if (find(closing[i].win) && closing[i].pid > 1) {
      WARN("pid %d did not close its window; sending SIGTERM", (int)closing[i].pid);
      kill(closing[i].pid, SIGTERM);
    }
    memmove(&closing[i], &closing[i + 1], (size_t)(closing_count - i - 1) * sizeof closing[0]);
    --closing_count;
  }
  return next;
}

/* ---------- keyboard ---------- */

static bool parse_keymod(const char* v) {
  if (!strcasecmp(v, "alt")) {
    switch_mod = KEYMOD_ALT;
    switch_mod_keys[0] = KEYCODE_LEFT_ALT;
    switch_mod_keys[1] = KEYCODE_RIGHT_ALT;
  } else if (!strcasecmp(v, "ralt")) {
    /* Right Alt is AltGr on most layouts. */
    switch_mod = KEYMOD_ALTGR;
    switch_mod_keys[0] = switch_mod_keys[1] = KEYCODE_RIGHT_ALT;
  } else if (!strcasecmp(v, "ctrl")) {
    switch_mod = KEYMOD_CTRL;
    switch_mod_keys[0] = KEYCODE_LEFT_CTRL;
    switch_mod_keys[1] = KEYCODE_RIGHT_CTRL;
  } else if (!strcasecmp(v, "shift")) {
    switch_mod = KEYMOD_SHIFT;
    switch_mod_keys[0] = KEYCODE_LEFT_SHIFT;
    switch_mod_keys[1] = KEYCODE_RIGHT_SHIFT;
  } else {
    return false;
  }
  return true;
}

static bool parse_hotkey(const char* v) {
  static const struct {
    const char* name;
    int key;
  } keys[] = {
      {"tab", KEYCODE_TAB}, {"space", KEYCODE_SPACE}, {"grave", KEYCODE_GRAVE},
      {"q", KEYCODE_Q},     {"w", KEYCODE_W},         {"t", KEYCODE_T},
      {"m", KEYCODE_M},
  };
  for (size_t i = 0; i < sizeof keys / sizeof keys[0]; ++i)
    if (!strcasecmp(v, keys[i].name)) {
      switch_key = keys[i].key;
      return true;
    }
  if ((v[0] == 'f' || v[0] == 'F') && isdigit((unsigned char)v[1])) {
    char* end = NULL;
    long n = strtol(v + 1, &end, 10);
    if (*end == 0 && n >= 1 && n <= 12) {
      switch_key = KEYCODE_F1 + (int)n - 1;
      return true;
    }
  }
  return false;
}

static void read_key_env(void) {
  const char* v = getenv("WINMGR_KEYMOD");
  if (v && *v && !parse_keymod(v)) WARN("invalid WINMGR_KEYMOD '%s'; using alt", v);
  v = getenv("WINMGR_HOTKEY");
  if (v && *v && !parse_hotkey(v)) WARN("invalid WINMGR_HOTKEY '%s'; using tab", v);
}

static void route_keyboard(screen_device_t device) {
  int type = 0;
  screen_get_device_property_iv(device, SCREEN_PROPERTY_TYPE, &type);
  if (type != SCREEN_EVENT_KEYBOARD) return;
  if (screen_set_device_property_pv(device, SCREEN_PROPERTY_SESSION, (void**)&key_session))
    WARN("cannot route a keyboard to the switcher: %s", strerror(errno));
  else
    LOG("keyboard %p routed through the switcher", (void*)device);
}

/* Every keyboard sends its events to a session of ours first; route_key()
 * passes the ones the switcher does not use to the focused window. */
static void create_key_session(void) {
  if (screen_create_session_type(&key_session, ctx, SCREEN_EVENT_KEYBOARD)) {
    WARN("cannot create a keyboard session, Alt+Tab is off: %s", strerror(errno));
    key_session = NULL;
    return;
  }
  int pos[2] = {0, 0};
  int z = 0x7fffffff;
  screen_set_session_property_iv(key_session, SCREEN_PROPERTY_POSITION, pos);
  screen_set_session_property_iv(key_session, SCREEN_PROPERTY_SIZE, display_size);
  screen_set_session_property_iv(key_session, SCREEN_PROPERTY_ZORDER, &z);
  screen_set_session_property_pv(key_session, SCREEN_PROPERTY_DISPLAY, (void**)&display);
  int count = 0;
  screen_get_context_property_iv(ctx, SCREEN_PROPERTY_DEVICE_COUNT, &count);
  if (count > 0) {
    screen_device_t* devices = calloc((size_t)count, sizeof *devices);
    if (devices) {
      screen_get_context_property_pv(ctx, SCREEN_PROPERTY_DEVICES, (void**)devices);
      for (int i = 0; i < count; ++i) route_keyboard(devices[i]);
      free(devices);
    }
  }
  screen_flush_context(ctx, 0);
}

/* Sends a key the switcher does not take on to the focused window. Without
 * a device the event is not routed back to our session. */
static void forward_key(screen_event_t ev) {
  screen_device_t none = NULL;
  screen_set_event_property_pv(ev, SCREEN_PROPERTY_DEVICE, (void**)&none);
  if (screen_inject_event(display, ev)) WARN("cannot pass a key on: %s", strerror(errno));
  screen_flush_context(ctx, 0);
}

static void swallow(int key) {
  for (int i = 0; i < swallowed_count; ++i)
    if (swallowed[i] == key) return;
  if (swallowed_count < (int)(sizeof swallowed / sizeof swallowed[0])) swallowed[swallowed_count++] = key;
}

/* True, and forgotten, when this is the release of a press the switcher
 * used. */
static bool unswallow(int key) {
  for (int i = 0; i < swallowed_count; ++i) {
    if (swallowed[i] != key) continue;
    swallowed[i] = swallowed[--swallowed_count];
    return true;
  }
  return false;
}

static void on_keyboard(screen_event_t ev) {
  int flags = 0, cap = 0, sym = 0, mods = 0;
  screen_get_event_property_iv(ev, SCREEN_PROPERTY_FLAGS, &flags);
  screen_get_event_property_iv(ev, SCREEN_PROPERTY_KEY_CAP, &cap);
  screen_get_event_property_iv(ev, SCREEN_PROPERTY_SYM, &sym);
  screen_get_event_property_iv(ev, SCREEN_PROPERTY_MODIFIERS, &mods);
  int key = (flags & SCREEN_FLAG_CAP_VALID) ? cap : sym;
  bool down = flags & SCREEN_FLAG_KEY_DOWN;
  bool held = mods & switch_mod;
  bool is_mod = key == switch_mod_keys[0] || key == switch_mod_keys[1];
  bool backwards = (mods & KEYMOD_SHIFT) && switch_mod != KEYMOD_SHIFT;
  LOG("key 0x%x %s mods=0x%x", key, down ? "down" : "up", mods);

  if (!down && unswallow(key)) return;
  if (switcher_open) {
    /* After a modifier key's release the event's modifiers say whether the
     * other one (left or right) is still held. */
    if ((is_mod && !down && !held) || (!held && !is_mod)) {
      /* Modifier released. The release goes to the app that saw the press,
       * which still has focus (the switcher never takes it); then switch. */
      forward_key(ev);
      activate_switcher();
      return;
    }
    if (is_mod || !down) {
      /* Modifier presses, and releases of keys pressed before the switcher
       * opened, go to the app as usual. */
      forward_key(ev);
      return;
    }
    if (key == switch_key) {
      step_switcher(backwards ? -1 : 1);
    } else if (key == KEYCODE_RIGHT) {
      step_switcher(1);
    } else if (key == KEYCODE_LEFT) {
      step_switcher(-1);
    } else if (key == KEYCODE_DELETE || key == KEYCODE_KP_DELETE) {
      managed_t* m = find(switch_list[switch_sel]);
      if (m) close_window(m);
    } else if (key == KEYCODE_ESCAPE) {
      LOG("switcher cancelled");
      hide_switcher();
      apply_stack(); /* focus back on the front window */
    }
    /* Every other press goes nowhere while the switcher is open, and its
     * release neither. */
    swallow(key);
    return;
  }
  if (down && held && !is_mod && key == switch_key && !(flags & SCREEN_FLAG_KEY_REPEAT)) {
    open_switcher(backwards);
    if (switcher_open) {
      swallow(key);
      return;
    }
  }
  forward_key(ev);
}

/* ---------- Screen events ---------- */

static void on_create(screen_window_t win) {
  int type = 0;
  screen_get_window_property_iv(win, SCREEN_PROPERTY_TYPE, &type);
  if (type != SCREEN_APPLICATION_WINDOW) return; /* child windows follow their parent */
  if (window_count >= MAX_WINDOWS) {
    WARN("too many windows; leaving one unmanaged");
    return;
  }
  managed_t* m = &windows[window_count++];
  memset(m, 0, sizeof *m);
  m->win = win;
  int pid = 0;
  screen_get_window_property_iv(win, SCREEN_PROPERTY_OWNER_PID, &pid);
  m->pid = pid;
  /* A launch tag belongs to the next window made within a few seconds. */
  if (pending_tag[0] && time(NULL) - pending_tag_time <= 15) {
    snprintf(m->tag, sizeof m->tag, "%s", pending_tag);
  }
  pending_tag[0] = 0;
  describe_window(m);
  /* Under a window manager Screen ignores an app's own POSITION, SIZE,
   * VISIBLE and ZORDER unless the manager grants self layout. Granting it is
   * what lets an app window keep, and later change, its size and place; the
   * manager still restacks on ZORDER changes (on_property). This happens
   * before the manager string is set, so an app that sees the answer can
   * already place itself. */
  int self_layout = 1;
  screen_set_window_property_iv(win, SCREEN_PROPERTY_SELF_LAYOUT, &self_layout);
  screen_set_window_property_cv(win, SCREEN_PROPERTY_MANAGER_STRING, (int)strlen(MANAGER_REPLY),
                                MANAGER_REPLY);
  stack_push_top(win);
  screen_flush_context(ctx, 0);
  LOG("create pid=%d tag='%s' title='%s'", pid, m->tag, m->title);
}

static void on_post(screen_window_t win) {
  managed_t* m = find(win);
  if (!m || m->posted) return;
  m->posted = true;
  m->post_seq = ++post_counter;
  if (m->launcher) {
    int size[2] = {display_size[0], display_size[1]};
    int pos[2] = {0, 0};
    screen_set_window_property_iv(win, SCREEN_PROPERTY_SIZE, size);
    screen_set_window_property_iv(win, SCREEN_PROPERTY_POSITION, pos);
  } else {
    /* Keep the app's size and position, only pulling the window back on
     * screen when it would open outside the display. */
    int size[2] = {0, 0}, pos[2] = {0, 0};
    screen_get_window_property_iv(win, SCREEN_PROPERTY_SIZE, size);
    screen_get_window_property_iv(win, SCREEN_PROPERTY_POSITION, pos);
    if (size[0] <= 0 || size[1] <= 0) {
      size[0] = display_size[0];
      size[1] = display_size[1];
      screen_set_window_property_iv(win, SCREEN_PROPERTY_SIZE, size);
    }
    int moved[2] = {pos[0], pos[1]};
    if (moved[0] + size[0] > display_size[0]) moved[0] = display_size[0] - size[0];
    if (moved[1] + size[1] > display_size[1]) moved[1] = display_size[1] - size[1];
    if (moved[0] < 0) moved[0] = 0;
    if (moved[1] < 0) moved[1] = 0;
    if (moved[0] != pos[0] || moved[1] != pos[1])
      screen_set_window_property_iv(win, SCREEN_PROPERTY_POSITION, moved);
    stack_push_top(win);
    LOG("post pid=%d size=%dx%d at %d,%d", (int)m->pid, size[0], size[1], moved[0], moved[1]);
    /* A second window of an app that already has one: nothing is killed (the
     * app or the launcher script owns that), but the first window stays in
     * front and the dock keeps one entry. */
    managed_t* first = first_window_of_app(m);
    if (first) {
      if (!m->duplicate_warned) {
        m->duplicate_warned = true;
        WARN("second window of '%s' (pid %d); the first is pid %d and stays in front", m->app,
             (int)m->pid, (int)first->pid);
      }
      raise_window(first);
    }
  }
  apply_stack();
}

static void on_close(screen_window_t win) {
  managed_t* m = find(win);
  if (!m) {
    screen_destroy_window(win); /* release this context's handle */
    return;
  }
  LOG("close pid=%d '%s'", (int)m->pid, m->title);
  stack_remove(win);
  closing_forget(win);
  switcher_forget(win);
  managed_t* launcher = launcher_window();
  if (m->tag[0] && launcher && launcher != m) send_manager_message(launcher->win, "closed", m->tag);
  free(m->icon);
  free(m->tile_icon);
  int index = (int)(m - windows);
  memmove(&windows[index], &windows[index + 1], (size_t)(window_count - index - 1) * sizeof windows[0]);
  --window_count;
  screen_destroy_window(win);
  apply_stack();
}

static void on_property(screen_window_t win, int name) {
  managed_t* m = find(win);
  if (!m) return;
  if (verbose && !m->launcher &&
      (name == SCREEN_PROPERTY_POSITION || name == SCREEN_PROPERTY_SIZE)) {
    /* Each place and size the app gives its window. During a left or top
     * edge resize the right and bottom edges must not move. */
    int size[2] = {0, 0}, pos[2] = {0, 0};
    screen_get_window_property_iv(win, SCREEN_PROPERTY_SIZE, size);
    screen_get_window_property_iv(win, SCREEN_PROPERTY_POSITION, pos);
    LOG("geometry pid=%d %dx%d+%d+%d right=%d bottom=%d", (int)m->pid, size[0], size[1], pos[0],
        pos[1], pos[0] + size[0], pos[1] + size[1]);
  }
  if (name == SCREEN_PROPERTY_MANAGER_STRING) {
    /* An app asking which manager runs: the property holds one value, so
     * answer by writing ours again when the app replaced it. */
    char current[128] = {0};
    screen_get_window_property_cv(win, SCREEN_PROPERTY_MANAGER_STRING, sizeof current - 1, current);
    if (strcmp(current, MANAGER_REPLY) != 0) {
      screen_set_window_property_cv(win, SCREEN_PROPERTY_MANAGER_STRING,
                                    (int)strlen(MANAGER_REPLY), MANAGER_REPLY);
      screen_flush_context(ctx, 0);
    }
  } else if (name == SCREEN_PROPERTY_VISIBLE && !m->launcher) {
    int visible = 1;
    screen_get_window_property_iv(win, SCREEN_PROPERTY_VISIBLE, &visible);
    LOG("visible=%d from pid=%d", visible, (int)m->pid);
    /* The manager writes VISIBLE too (minimize); those writes reach this
     * context as no events. A 0 the manager did not ask for is the app
     * hiding itself; a 1 after that is the app showing itself again. */
    if (!visible && !m->minimized && !m->hidden_by_app) {
      m->hidden_by_app = true;
      apply_stack();
    } else if (visible && (m->hidden_by_app || m->minimized)) {
      m->hidden_by_app = false;
      m->minimized = false;
      stack_push_top(win);
      apply_stack();
    }
  } else if (name == SCREEN_PROPERTY_ZORDER && !m->launcher) {
    /* Qt and other toolkits set their own z-order; the stack decides. */
    int z = -1;
    screen_get_window_property_iv(win, SCREEN_PROPERTY_ZORDER, &z);
    for (int i = 0; i < stack_count; ++i)
      if (stack[i] == win && z != APP_Z_BASE + i) {
        LOG("zorder %d from pid=%d; restacking", z, (int)m->pid);
        apply_stack();
      }
  } else if (name == SCREEN_PROPERTY_FOCUS && !m->launcher) {
    int focus = 0;
    screen_get_window_property_iv(win, SCREEN_PROPERTY_FOCUS, &focus);
    LOG("focus=%d pid=%d", focus, (int)m->pid);
    if (focus && front_window() != m) raise_window(m);
  }
}

/* Screen moves input focus to the window the user clicks. A click on an app
 * window that sits behind another brings it to the front. */
static void on_display_property(screen_event_t ev) {
  int name = 0;
  screen_get_event_property_iv(ev, SCREEN_PROPERTY_NAME, &name);
  if (name != SCREEN_PROPERTY_FOCUS) return;
  screen_window_t focus = NULL;
  if (screen_get_display_property_pv(display, SCREEN_PROPERTY_FOCUS, (void**)&focus) || !focus) return;
  managed_t* m = find(focus);
  LOG("display focus -> %p (%s)", (void*)focus, m ? m->title : "unmanaged");
  if (m && !m->launcher && !m->minimized && front_window() != m) raise_window(m);
}

static void on_dock_pointer(screen_event_t ev) {
  static int last_buttons;
  int buttons = 0, pos[2] = {0, 0};
  screen_get_event_property_iv(ev, SCREEN_PROPERTY_BUTTONS, &buttons);
  /* On this Screen build a pointer event delivered to the manager carries
   * display coordinates in POSITION (SOURCE_POSITION is window-relative);
   * the dock's hit areas are kept in display coordinates. */
  screen_get_event_property_iv(ev, SCREEN_PROPERTY_POSITION, pos);
  bool pressed = (buttons & SCREEN_LEFT_MOUSE_BUTTON) && !(last_buttons & SCREEN_LEFT_MOUSE_BUTTON);
  last_buttons = buttons;
  if (!pressed) return;
  if (switcher_open && pos[0] >= switcher_rect[0] && pos[0] < switcher_rect[0] + switcher_rect[2] &&
      pos[1] >= switcher_rect[1] && pos[1] < switcher_rect[1] + switcher_rect[3]) {
    /* A click on a switcher tile switches to it. */
    for (int i = 0; i < switch_count; ++i) {
      if (tile_x[i] < 0 || pos[0] < tile_x[i] || pos[0] >= tile_x[i] + TILE_W || pos[1] < tile_y[i] ||
          pos[1] >= tile_y[i] + TILE_H)
        continue;
      switch_sel = i;
      activate_switcher();
      return;
    }
    return;
  }
  if (switcher_open) {
    /* A click outside the panel closes it, as Escape does. */
    hide_switcher();
    apply_stack();
    return;
  }
  LOG("dock press at %d,%d", pos[0], pos[1]);
  managed_t* entries[MAX_WINDOWS];
  int n = dock_entries(entries);
  for (int i = 0; i < n; ++i) {
    managed_t* m = entries[i];
    if (pos[0] < m->dock_x || pos[0] >= m->dock_x + m->dock_w) continue;
    /* The entry stands for the app; act on its front window if a duplicate
     * is the one in front. */
    managed_t* front = front_window();
    managed_t* target = entry_is_front(m, front) ? front : m;
    if (target->minimized || front != target)
      raise_window(target);
    else
      minimize_window(target);
    return;
  }
}

static void handle_event(screen_event_t ev) {
  int type = SCREEN_EVENT_NONE;
  screen_get_event_property_iv(ev, SCREEN_PROPERTY_TYPE, &type);
  int object = -1;
  screen_window_t win = NULL;
  switch (type) {
    case SCREEN_EVENT_CREATE:
    case SCREEN_EVENT_POST:
    case SCREEN_EVENT_CLOSE:
    case SCREEN_EVENT_PROPERTY:
      screen_get_event_property_iv(ev, SCREEN_PROPERTY_OBJECT_TYPE, &object);
      if (object == SCREEN_OBJECT_TYPE_DISPLAY) {
        on_display_property(ev);
        return;
      }
      if (object == SCREEN_OBJECT_TYPE_DEVICE && type == SCREEN_EVENT_CREATE && key_session) {
        /* A keyboard plugged in later goes through the switcher too. */
        screen_device_t device = NULL;
        screen_get_event_property_pv(ev, SCREEN_PROPERTY_DEVICE, (void**)&device);
        if (device) {
          route_keyboard(device);
          screen_flush_context(ctx, 0);
        }
        return;
      }
      if (object != SCREEN_OBJECT_TYPE_WINDOW) return;
      screen_get_event_property_pv(ev, SCREEN_PROPERTY_WINDOW, (void**)&win);
      if (!win || win == dock || win == switcher) return;
      break;
    default:
      break;
  }
  switch (type) {
    case SCREEN_EVENT_CREATE:
      on_create(win);
      break;
    case SCREEN_EVENT_POST:
      on_post(win);
      break;
    case SCREEN_EVENT_CLOSE:
      on_close(win);
      break;
    case SCREEN_EVENT_PROPERTY: {
      int name = 0;
      screen_get_event_property_iv(ev, SCREEN_PROPERTY_NAME, &name);
      if (verbose && name != SCREEN_PROPERTY_MANAGER_STRING)
        LOG("property %d on %p", name, (void*)win);
      on_property(win, name);
      break;
    }
    case SCREEN_EVENT_MANAGER: {
      char data[512] = {0};
      screen_window_t from = NULL;
      screen_get_event_property_pv(ev, SCREEN_PROPERTY_WINDOW, (void**)&from);
      /* SIZE carries the payload length; asking for more than that returns
       * nothing. */
      int length[2] = {0, 0};
      screen_get_event_property_iv(ev, SCREEN_PROPERTY_SIZE, length);
      if (length[0] <= 0 || length[0] > (int)sizeof data - 1) length[0] = (int)sizeof data - 1;
      screen_get_event_property_cv(ev, SCREEN_PROPERTY_USER_DATA, length[0], data);
      handle_manager_message(from, data);
      break;
    }
    case SCREEN_EVENT_POINTER:
      /* The manager's windows are the dock and the switcher; every pointer
       * event that reaches this context is for one of them. */
      on_dock_pointer(ev);
      break;
    case SCREEN_EVENT_KEYBOARD:
      on_keyboard(ev);
      break;
    case SCREEN_EVENT_NONE:
      break; /* screen_get_event timed out for a close deadline */
    default:
      LOG("event type %d", type);
      break;
  }
}

int main(int argc, char** argv) {
  int opt;
  while ((opt = getopt(argc, argv, "vdms:l:")) != -1) {
    if (opt == 'v') verbose = true;
    else if (opt == '?') {
      fprintf(stderr, "usage: %s [-v] [-s apps] [-d] [-m] [-l n]\n", argv[0]);
      return 2;
    }
  }
  read_key_env();
  /* Input provider too: keys the switcher does not use are injected again
   * for the focused window. */
  if (screen_create_context(&ctx, SCREEN_WINDOW_MANAGER_CONTEXT | SCREEN_INPUT_PROVIDER_CONTEXT)) {
    WARN("cannot create a window manager context: %s", strerror(errno));
    return 1;
  }
  int count = 0;
  screen_get_context_property_iv(ctx, SCREEN_PROPERTY_DISPLAY_COUNT, &count);
  if (count < 1) {
    WARN("no display");
    return 1;
  }
  screen_display_t* displays = calloc((size_t)count, sizeof *displays);
  if (!displays) return 1;
  screen_get_context_property_pv(ctx, SCREEN_PROPERTY_DISPLAYS, (void**)displays);
  display = displays[0]; /* the first display; others are left alone */
  free(displays);
  screen_get_display_property_iv(display, SCREEN_PROPERTY_SIZE, display_size);
  LOG("display %dx%d", display_size[0], display_size[1]);

  load_font();
  create_dock();
  create_switcher();
  create_key_session();

  screen_event_t ev;
  if (screen_create_event(&ev)) return 1;
  for (;;) {
    int64_t timeout = check_closing();
    if (screen_get_event(ctx, ev, timeout < 0 ? (uint64_t)-1 : (uint64_t)timeout)) {
      if (errno == EINTR) continue;
      WARN("screen_get_event: %s", strerror(errno));
      return 1;
    }
    handle_event(ev);
  }
}
