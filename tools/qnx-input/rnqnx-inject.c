/* rnqnx-inject: scripted mouse and keyboard input for QNX Screen, for device
 * smoke tests. Not part of a shipped app.
 *
 * It creates its own pointer and keyboard devices as an input provider and
 * injects events through them, so Screen routes them like a USB mouse or
 * keyboard: it picks the window under the pointer, converts the position to
 * window coordinates, and sends the event to that window's owner.
 *
 * Two properties make that work, and both were missing from the first
 * injector that sent events with no device:
 *   - The event needs SCREEN_FLAG_POSITION_VALID in SCREEN_PROPERTY_FLAGS.
 *     Without it Screen ignores POSITION and treats the pointer as being at
 *     0,0 on the display (window-relative -319,-139 for a window at 319,139),
 *     and Qt gets coordinates outside the window.
 *   - The event needs SCREEN_PROPERTY_DEVICE set to a pointer device that was
 *     created with screen_create_device_type().
 * POSITION is display coordinates. Run as root (the input provider context).
 *
 * Usage: rnqnx-inject <command> [args] [<command> [args]] ...
 *   move X Y                 move the pointer
 *   down [left|middle|right] press a button at the current position
 *   up                       release all buttons
 *   click X Y [button]       move, press, release
 *   dclick X Y               two clicks 90 ms apart
 *   drag X0 Y0 X1 Y1 [steps] press at X0,Y0, move in steps, release at X1,Y1
 *   wheel N                  N wheel ticks at the current position (up < 0)
 *   hwheel N                 horizontal wheel ticks
 *   mods HEX                 modifier mask for later pointer and key events
 *   key NAME|HEX [mods]      key press and release (NAME: esc tab ret space
 *                            bksp del left right up down home end pgup pgdn
 *                            alt ctrl shift, or one character)
 *   keydown NAME|HEX, keyup NAME|HEX
 *   type TEXT                type ASCII text
 *   sleep MS
 * Without arguments it prints this text. */
#include <screen/screen.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static screen_context_t ctx;
static screen_display_t display;
static screen_device_t pointerDevice;
static screen_device_t keyboardDevice;
static int curX, curY, curButtons, curMods;

static void pause_ms(int ms) { usleep((useconds_t)ms * 1000); }

/* Screen leaves the timestamp of an injected event at 0. Qt takes double
 * clicks and flick velocity from event timestamps, so with 0 every click
 * pair is a double click and no drag has a speed. Real mouse events carry
 * milliseconds. */
static long long now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int send_pointer(int wheel, int hwheel) {
  screen_event_t ev;
  if (screen_create_event(&ev)) return perror("screen_create_event"), 1;
  int type = SCREEN_EVENT_POINTER;
  int pos[2] = {curX, curY};
  int flags = SCREEN_FLAG_POSITION_VALID;
  screen_set_event_property_iv(ev, SCREEN_PROPERTY_TYPE, &type);
  screen_set_event_property_pv(ev, SCREEN_PROPERTY_DEVICE, (void**)&pointerDevice);
  screen_set_event_property_iv(ev, SCREEN_PROPERTY_FLAGS, &flags);
  screen_set_event_property_iv(ev, SCREEN_PROPERTY_POSITION, pos);
  screen_set_event_property_iv(ev, SCREEN_PROPERTY_BUTTONS, &curButtons);
  screen_set_event_property_iv(ev, SCREEN_PROPERTY_MODIFIERS, &curMods);
  long long stamp = now_ms();
  screen_set_event_property_llv(ev, SCREEN_PROPERTY_TIMESTAMP, &stamp);
  if (wheel) screen_set_event_property_iv(ev, SCREEN_PROPERTY_MOUSE_WHEEL, &wheel);
  if (hwheel) screen_set_event_property_iv(ev, SCREEN_PROPERTY_MOUSE_HORIZONTAL_WHEEL, &hwheel);
  int rc = screen_inject_event(display, ev);
  if (rc) perror("screen_inject_event(pointer)");
  screen_flush_context(ctx, 0);
  screen_destroy_event(ev);
  return rc;
}

static int move_to(int x, int y) {
  curX = x;
  curY = y;
  return send_pointer(0, 0);
}

static int set_buttons(int buttons) {
  curButtons = buttons;
  return send_pointer(0, 0);
}

static int send_key(int sym, int down) {
  screen_event_t ev;
  if (screen_create_event(&ev)) return perror("screen_create_event"), 1;
  int type = SCREEN_EVENT_KEYBOARD;
  int flags = SCREEN_FLAG_SYM_VALID | SCREEN_FLAG_CAP_VALID | (down ? SCREEN_FLAG_KEY_DOWN : 0);
  screen_set_event_property_iv(ev, SCREEN_PROPERTY_TYPE, &type);
  screen_set_event_property_pv(ev, SCREEN_PROPERTY_DEVICE, (void**)&keyboardDevice);
  screen_set_event_property_iv(ev, SCREEN_PROPERTY_FLAGS, &flags);
  screen_set_event_property_iv(ev, SCREEN_PROPERTY_SYM, &sym);
  screen_set_event_property_iv(ev, SCREEN_PROPERTY_KEY_CAP, &sym);
  screen_set_event_property_iv(ev, SCREEN_PROPERTY_MODIFIERS, &curMods);
  long long stamp = now_ms();
  screen_set_event_property_llv(ev, SCREEN_PROPERTY_TIMESTAMP, &stamp);
  int rc = screen_inject_event(display, ev);
  if (rc) perror("screen_inject_event(key)");
  screen_flush_context(ctx, 0);
  screen_destroy_event(ev);
  return rc;
}

static int key_sym(const char* name) {
  static const struct { const char* name; int sym; } names[] = {
      {"esc", 0xf01b},  {"tab", 0xf009},   {"ret", 0xf00d}, {"enter", 0xf00d}, {"space", 0x20},
      {"bksp", 0xf008}, {"del", 0xf0ff},   {"left", 0xf051}, {"right", 0xf053}, {"up", 0xf052},
      {"down", 0xf054}, {"home", 0xf050},  {"end", 0xf057},  {"pgup", 0xf055},  {"pgdn", 0xf056},
      {"alt", 0xf0e9},  {"ctrl", 0xf0e3},  {"shift", 0xf0e1},
  };
  for (size_t i = 0; i < sizeof names / sizeof names[0]; ++i)
    if (!strcmp(names[i].name, name)) return names[i].sym;
  if (name[0] && !name[1]) return (unsigned char)name[0];
  return (int)strtol(name, NULL, 16);
}

static int button_mask(const char* name) {
  if (!name) return SCREEN_LEFT_MOUSE_BUTTON;
  if (!strcmp(name, "right")) return SCREEN_RIGHT_MOUSE_BUTTON;
  if (!strcmp(name, "middle")) return SCREEN_MIDDLE_MOUSE_BUTTON;
  return SCREEN_LEFT_MOUSE_BUTTON;
}

static int is_button(const char* s) {
  return s && (!strcmp(s, "left") || !strcmp(s, "right") || !strcmp(s, "middle"));
}

static void usage(const char* argv0) {
  fprintf(stderr,
          "usage: %s <command> [args] ...\n"
          "  move X Y | down [left|middle|right] | up | click X Y [button] | dclick X Y\n"
          "  drag X0 Y0 X1 Y1 [steps] | wheel N | hwheel N | mods HEX\n"
          "  key NAME|HEX [mods] | keydown NAME|HEX | keyup NAME|HEX | type TEXT | sleep MS\n"
          "Coordinates are display pixels. Run as root. See the comment at the top of\n"
          "tools/qnx-input/rnqnx-inject.c.\n",
          argv0);
}

int main(int argc, char** argv) {
  if (argc < 2) return usage(argv[0]), 2;
  if (screen_create_context(&ctx, SCREEN_INPUT_PROVIDER_CONTEXT)) return perror("screen_create_context"), 1;
  int count = 0;
  screen_get_context_property_iv(ctx, SCREEN_PROPERTY_DISPLAY_COUNT, &count);
  if (count < 1) return fprintf(stderr, "no display\n"), 1;
  screen_display_t displays[8];
  if (count > 8) count = 8;
  screen_get_context_property_pv(ctx, SCREEN_PROPERTY_DISPLAYS, (void**)displays);
  display = displays[0];
  if (screen_create_device_type(&pointerDevice, ctx, SCREEN_EVENT_POINTER) ||
      screen_create_device_type(&keyboardDevice, ctx, SCREEN_EVENT_KEYBOARD))
    return perror("screen_create_device_type"), 1;
  screen_set_device_property_pv(pointerDevice, SCREEN_PROPERTY_DISPLAY, (void**)&display);
  screen_set_device_property_pv(keyboardDevice, SCREEN_PROPERTY_DISPLAY, (void**)&display);
  screen_flush_context(ctx, SCREEN_WAIT_IDLE);
  pause_ms(400); /* the window manager routes new keyboards on a device event */

  int rc = 0;
  for (int i = 1; i < argc && !rc;) {
    const char* c = argv[i++];
    int left = argc - i;
    if (!strcmp(c, "move") && left >= 2) {
      rc = move_to(atoi(argv[i]), atoi(argv[i + 1]));
      i += 2;
    } else if (!strcmp(c, "down")) {
      rc = set_buttons(curButtons | button_mask(left > 0 && is_button(argv[i]) ? argv[i++] : NULL));
    } else if (!strcmp(c, "up")) {
      rc = set_buttons(0);
    } else if ((!strcmp(c, "click") || !strcmp(c, "dclick")) && left >= 2) {
      int x = atoi(argv[i]), y = atoi(argv[i + 1]);
      i += 2;
      int mask = button_mask(!strcmp(c, "click") && left > 2 && is_button(argv[i]) ? argv[i++] : NULL);
      int times = !strcmp(c, "dclick") ? 2 : 1;
      rc = move_to(x, y);
      pause_ms(60);
      for (int n = 0; n < times && !rc; ++n) {
        rc = set_buttons(mask);
        pause_ms(60);
        rc |= set_buttons(0);
        pause_ms(90);
      }
    } else if (!strcmp(c, "drag") && left >= 4) {
      int x0 = atoi(argv[i]), y0 = atoi(argv[i + 1]), x1 = atoi(argv[i + 2]), y1 = atoi(argv[i + 3]);
      i += 4;
      int steps = left > 4 ? atoi(argv[i++]) : 12;
      if (steps < 1) steps = 1;
      rc = move_to(x0, y0);
      pause_ms(80);
      rc |= set_buttons(SCREEN_LEFT_MOUSE_BUTTON);
      pause_ms(80);
      for (int s = 1; s <= steps && !rc; ++s) {
        rc = move_to(x0 + (x1 - x0) * s / steps, y0 + (y1 - y0) * s / steps);
        pause_ms(30);
      }
      pause_ms(80);
      rc |= set_buttons(0);
    } else if ((!strcmp(c, "wheel") || !strcmp(c, "hwheel")) && left >= 1) {
      int n = atoi(argv[i++]);
      int step = n < 0 ? -1 : 1;
      for (int k = 0; k != n && !rc; k += step) {
        rc = !strcmp(c, "wheel") ? send_pointer(step, 0) : send_pointer(0, step);
        pause_ms(40);
      }
    } else if (!strcmp(c, "mods") && left >= 1) {
      curMods = (int)strtol(argv[i++], NULL, 16);
    } else if (!strcmp(c, "key") && left >= 1) {
      int sym = key_sym(argv[i++]);
      if (left >= 2 && !is_button(argv[i]) && strtol(argv[i], NULL, 16) >= 0 && argv[i][0] >= '0' &&
          argv[i][0] <= '9')
        curMods = (int)strtol(argv[i++], NULL, 16);
      rc = send_key(sym, 1);
      pause_ms(60);
      rc |= send_key(sym, 0);
      pause_ms(60);
    } else if ((!strcmp(c, "keydown") || !strcmp(c, "keyup")) && left >= 1) {
      rc = send_key(key_sym(argv[i++]), !strcmp(c, "keydown"));
      pause_ms(60);
    } else if (!strcmp(c, "type") && left >= 1) {
      for (const char* p = argv[i++]; *p && !rc; ++p) {
        rc = send_key((unsigned char)*p, 1);
        pause_ms(30);
        rc |= send_key((unsigned char)*p, 0);
        pause_ms(30);
      }
    } else if (!strcmp(c, "sleep") && left >= 1) {
      pause_ms(atoi(argv[i++]));
    } else {
      fprintf(stderr, "bad command or arguments: %s\n", c);
      usage(argv[0]);
      rc = 2;
    }
  }
  pause_ms(200);
  screen_destroy_device(pointerDevice);
  screen_destroy_device(keyboardDevice);
  screen_destroy_context(ctx);
  return rc;
}
