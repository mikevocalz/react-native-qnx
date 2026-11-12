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
 *     windowed", so an app can tell it may use a real window of its own size.
 * Apps send requests the way the launcher does, as manager messages naming
 * their window:
 *   "rnqnx=minimize"  hide the window (it stays in the dock)
 *   "rnqnx=restore"   show and raise it
 *   "rnqnx=raise"     raise it
 *
 * Usage: rnqnx-winmgr [-v] [-s list] [-d] [-m] [-l n]
 *   -v  log every event to stderr. -s, -d, -m and -l are accepted for
 *   command-line compatibility with fullscreen-winmgr and ignored.
 */

#include <screen/screen.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <png.h>

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

typedef struct {
  screen_window_t win;
  pid_t pid;
  bool launcher;
  bool posted;
  bool minimized;
  bool hidden_by_app; /* the app itself set VISIBLE=0 */
  char tag[128];
  char title[128];
  uint32_t* icon; /* ITEM_ICON x ITEM_ICON premultiplied BGRA, or NULL */
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

static char pending_tag[128];
static time_t pending_tag_time;

static screen_window_t dock;
static int dock_rect[4]; /* x, y, w, h on the display */
static FT_Library ft;
static FT_Face face;

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

static uint32_t* load_icon(const char* path) {
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
  uint32_t* out = calloc(ITEM_ICON * ITEM_ICON, sizeof *out);
  if (out) {
    /* Box filter down to ITEM_ICON, premultiplied. */
    for (int y = 0; y < ITEM_ICON; ++y) {
      for (int x = 0; x < ITEM_ICON; ++x) {
        int x0 = x * (int)image.width / ITEM_ICON, x1 = (x + 1) * (int)image.width / ITEM_ICON;
        int y0 = y * (int)image.height / ITEM_ICON, y1 = (y + 1) * (int)image.height / ITEM_ICON;
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
        out[y * ITEM_ICON + x] = (uint32_t)((a / n) << 24 | (r / n / 255) << 16 |
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
  m->icon = load_icon(icon);
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

static void draw_monogram(uint32_t* px, int stride, int bw, int bh, int x, int y, const char* title) {
  fill_round_rect(px, stride, bw, bh, x, y, ITEM_ICON, ITEM_ICON, 8, 0xff3a6ea5);
  char letter[2] = {(char)toupper((unsigned char)(title[0] ? title[0] : '?')), 0};
  int tw = text_width(letter);
  draw_text(px, stride, bw, bh, x + (ITEM_ICON - tw) / 2, y + ITEM_ICON / 2 + 6, letter, ITEM_ICON,
            0xffffff);
}

/* Entries shown in the dock: posted app windows the app has not hidden. */
static int dock_entries(managed_t** out) {
  int n = 0;
  for (int i = 0; i < window_count; ++i) {
    managed_t* m = &windows[i];
    if (m->launcher || !m->posted || m->hidden_by_app) continue;
    out[n++] = m;
  }
  return n;
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
  if (width > display_size[0] - 2 * DOCK_MARGIN) width = display_size[0] - 2 * DOCK_MARGIN;
  int height = DOCK_HEIGHT;
  dock_rect[0] = (display_size[0] - width) / 2;
  dock_rect[1] = display_size[1] - LAUNCHER_BANNER - height - DOCK_MARGIN;
  dock_rect[2] = width;
  dock_rect[3] = height;

  /* Buffers are made once at the largest size the dock can have. */
  screen_buffer_t buffer;
  if (screen_get_window_property_pv(dock, SCREEN_PROPERTY_RENDER_BUFFERS, (void**)&buffer)) return;
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
    uint32_t bg = m == front ? 0xff2f4258 : 0x00000000;
    if (bg) fill_round_rect(px, stride, width, height, x, DOCK_PAD, w, height - 2 * DOCK_PAD, 10, bg);
    int iy = (height - ITEM_ICON) / 2;
    if (m->icon) {
      for (int yy = 0; yy < ITEM_ICON; ++yy)
        for (int xx = 0; xx < ITEM_ICON; ++xx) {
          uint32_t* d = &px[(iy + yy) * stride + x + 8 + xx];
          uint32_t s = m->icon[yy * ITEM_ICON + xx];
          /* Half opacity for a minimized window (premultiplied, so every
           * channel halves). */
          if (m->minimized) s = s >> 1 & 0x7f7f7f7f;
          *d = blend(*d, s);
        }
    } else {
      draw_monogram(px, stride, width, height, x + 8, iy, m->title);
    }
    draw_text(px, stride, width, height, x + 8 + ITEM_ICON + 10, height / 2 + 6, m->title,
              w - (8 + ITEM_ICON + 10) - 8, m->minimized ? 0x8a96a3 : 0xf2f5f8);
    if (m == front) fill_round_rect(px, stride, width, height, x + w / 2 - 10, height - 5, 20, 3, 1,
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
  char* save = NULL;
  for (char* key = strtok_r(copy, "=", &save); key; key = strtok_r(NULL, "=", &save)) {
    char* value = strtok_r(NULL, "=", &save);
    if (!value) break;
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
  }
  apply_stack();
}

static void on_close(screen_window_t win) {
  managed_t* m = find(win);
  if (!m) return;
  LOG("close pid=%d '%s'", (int)m->pid, m->title);
  stack_remove(win);
  managed_t* launcher = launcher_window();
  if (m->tag[0] && launcher && launcher != m) send_manager_message(launcher->win, "closed", m->tag);
  free(m->icon);
  int index = (int)(m - windows);
  memmove(&windows[index], &windows[index + 1], (size_t)(window_count - index - 1) * sizeof windows[0]);
  --window_count;
  screen_destroy_window(win);
  apply_stack();
}

static void on_property(screen_window_t win, int name) {
  managed_t* m = find(win);
  if (!m) return;
  if (name == SCREEN_PROPERTY_MANAGER_STRING) {
    /* An app asking which manager runs: the property holds one value, so
     * answer by writing ours again. */
    screen_set_window_property_cv(win, SCREEN_PROPERTY_MANAGER_STRING, (int)strlen(MANAGER_REPLY),
                                  MANAGER_REPLY);
    screen_flush_context(ctx, 0);
  } else if (name == SCREEN_PROPERTY_VISIBLE && !m->launcher) {
    int visible = 1;
    screen_get_window_property_iv(win, SCREEN_PROPERTY_VISIBLE, &visible);
    LOG("visible=%d from pid=%d", visible, (int)m->pid);
    /* The manager also writes VISIBLE; only a change it did not make counts
     * as the app hiding or showing itself. */
    bool expected = !m->minimized;
    if (visible != expected) {
      m->hidden_by_app = !visible;
      if (visible) {
        m->minimized = false;
        stack_push_top(win);
      }
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
  LOG("dock press at %d,%d", pos[0], pos[1]);
  managed_t* entries[MAX_WINDOWS];
  int n = dock_entries(entries);
  for (int i = 0; i < n; ++i) {
    managed_t* m = entries[i];
    if (pos[0] < m->dock_x || pos[0] >= m->dock_x + m->dock_w) continue;
    if (m->minimized || front_window() != m)
      raise_window(m);
    else
      minimize_window(m);
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
      if (object != SCREEN_OBJECT_TYPE_WINDOW) return;
      screen_get_event_property_pv(ev, SCREEN_PROPERTY_WINDOW, (void**)&win);
      if (!win || win == dock) return;
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
      /* The manager's only window is the dock, so every pointer event that
       * reaches this context is for it. */
      on_dock_pointer(ev);
      break;
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
  if (screen_create_context(&ctx, SCREEN_WINDOW_MANAGER_CONTEXT)) {
    WARN("cannot create a window manager context: %s", strerror(errno));
    return 1;
  }
  int count = 0;
  screen_get_context_property_iv(ctx, SCREEN_PROPERTY_DISPLAY_COUNT, &count);
  if (count < 1) {
    WARN("no display");
    return 1;
  }
  screen_display_t displays[16];
  if (count > 16) count = 16;
  screen_get_context_property_pv(ctx, SCREEN_PROPERTY_DISPLAYS, (void**)displays);
  display = displays[0];
  screen_get_display_property_iv(display, SCREEN_PROPERTY_SIZE, display_size);
  LOG("display %dx%d", display_size[0], display_size[1]);

  load_font();
  create_dock();

  screen_event_t ev;
  if (screen_create_event(&ev)) return 1;
  for (;;) {
    if (screen_get_event(ctx, ev, -1)) {
      if (errno == EINTR) continue;
      WARN("screen_get_event: %s", strerror(errno));
      return 1;
    }
    handle_event(ev);
  }
}
