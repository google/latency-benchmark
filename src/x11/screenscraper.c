/*
 * Copyright 2013 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "../screenscraper.h"
#include "../latency-benchmark.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>  // XGetPixel, XDestroyImage
#include <X11/keysym.h> // XK_Z
#include <X11/XKBlib.h>
#include <X11/extensions/XTest.h>
#include <GL/glx.h>
#include <stddef.h>
#include <sys/time.h>   // gettimeofday
#include <string.h>     // memset
#include <math.h>
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>



static Display *display = NULL;

#define min(X, Y) ((X) < (Y) ? (X) : (Y))
#define max(X, Y) ((X) > (Y) ? (X) : (Y))


static int clamp(int value, int minimum, int maximum) {
  return max(min(value, maximum), minimum);
}


screenshot *take_screenshot(uint32_t x, uint32_t y, uint32_t width,
    uint32_t height) {
  if (!display) {
    display = XOpenDisplay(NULL);
    if (!display) {
      return false;
    }
  }
  // Make sure width and height can be safely converted to signed integers.
  width = min(width, INT_MAX);
  height = min(height, INT_MAX);
  int display_x, display_y;
  unsigned int display_width, display_height, border_width, display_depth;
  Window root;
  XGetGeometry(display, RootWindow(display, 0), &root, &display_x, &display_y,
      &display_width, &display_height, &border_width, &display_depth);
  // TODO: can these be non-zero?
  assert(display_x == 0);
  assert(display_y == 0);
  x = clamp(x, 0, display_width);
  y = clamp(y, 0, display_width);
  int clamped_width = clamp(width, 0, display_width - x);
  int clamped_height = clamp(height, 0, display_height - y);
  if (clamped_width == 0 || clamped_height == 0) {
    debug_log("screenshot rect empty");
    return NULL;
  }
  XImage *image = XGetImage(display, RootWindow(display, 0), x, y,
      clamped_width, clamped_height, AllPlanes, ZPixmap);
  assert(image);
  assert(image->width == clamped_width);
  assert(image->height == clamped_height);
  assert(image->byte_order == LSBFirst);
  assert(image->bits_per_pixel == 32);
  assert(image->red_mask == 0x00FF0000);
  assert(image->green_mask == 0x0000FF00);
  assert(image->blue_mask == 0x000000FF);
  screenshot *shot = (screenshot *) malloc(sizeof(screenshot));
  shot->width = image->width;
  shot->height = image->height;
  shot->stride = image->bytes_per_line;
  shot->pixels = (uint8_t *)image->data;
  shot->time_nanoseconds = get_nanoseconds();
  shot->platform_specific_data = image;
  return shot;
}


void free_screenshot(screenshot *shot) {
  XDestroyImage((XImage *)shot->platform_specific_data);
  free(shot);
}


static bool send_keystroke(int keysym) {
  if (!display) {
    display = XOpenDisplay(NULL);
    if (!display) {
      return false;
    }
  }
  // Send a keydown event for the 'Z' key, followed immediately by keyup.
  XKeyEvent event;
  memset(&event, 0, sizeof(XKeyEvent));
  Window focused;
  int ignored;
  XGetInputFocus(display, &focused, &ignored);
  event.display = display;
  event.root = RootWindow(display, 0);
  event.window = focused;
  event.subwindow = None;
  event.time = CurrentTime;
  event.same_screen = True;
  event.keycode = XKeysymToKeycode(display, keysym);
  event.type = KeyPress;
  XSendEvent(display, focused, True, KeyPressMask, (XEvent*) &event);
  event.type = KeyRelease;
  XSendEvent(display, focused, True, KeyReleaseMask, (XEvent*) &event);
  XSync(display, False);
  return true;
}


bool send_keystroke_b() { return send_keystroke(XK_B); }
bool send_keystroke_t() { return send_keystroke(XK_T); }
bool send_keystroke_w() { return send_keystroke(XK_W); }
bool send_keystroke_z() { return send_keystroke(XK_Z); }


bool send_scroll_down(int x, int y) {
  if (!display) {
    display = XOpenDisplay(NULL);
    if (!display) {
      return false;
    }
  }
  XWarpPointer(display, None, RootWindow(display, 0), 0, 0, 0, 0, x, y);
  static bool x_test_extension_queried = false;
  static bool x_test_extension_available = false;
  if (!x_test_extension_queried) {
    x_test_extension_queried = true;
    int ignored;
    x_test_extension_available = XTestQueryExtension(display, &ignored,
        &ignored, &ignored, &ignored);
  }
  if (!x_test_extension_available) {
    debug_log("XTest extension not available.");
    return false;
    // TODO: figure out why XSendEvent isn't working. XTest shouldn't be
    // required.
    // XButtonEvent event;
    // memset(&event, 0, sizeof(XButtonEvent));
    // Window focused;
    // int ignored;
    // XGetInputFocus(display, &focused, &ignored);
    // event.display = display;
    // event.root = RootWindow(display, 0);
    // event.window = focused;
    // event.subwindow = None;
    // event.time = CurrentTime;
    // event.same_screen = True;
    // event.type = ButtonPress;
    // // TODO: calculate these correctly, they should be relative to the
    // // focused window
    // event.x = x;
    // event.y = y;
    // event.x_root = 1;
    // event.y_root = 1;
    // event.button = Button5;
    // XSendEvent(display, focused, True, ButtonPressMask, (XEvent*) &event);
    // event.state = Button5Mask;
    // event.type = ButtonRelease;
    // XSendEvent(display, focused, True, ButtonReleaseMask, (XEvent*) &event);
  }
  XTestFakeButtonEvent(display, Button5, true, CurrentTime);
  XTestFakeButtonEvent(display, Button5, false, CurrentTime);
  XSync(display, False);
  return true;
}


static bool start_time_initialized = false;
static struct timeval start_time;
static const int64_t nanoseconds_per_microsecond = 1000;
int64_t get_nanoseconds() {
  if (!start_time_initialized) {
    gettimeofday(&start_time, NULL);
    start_time_initialized = true;
  }
  struct timeval current_time, difference;
  gettimeofday(&current_time, NULL);
  timersub(&current_time, &start_time, &difference);
  return ((int64_t)difference.tv_sec) * nanoseconds_per_second +
      ((int64_t)difference.tv_usec) * nanoseconds_per_microsecond;
}


void debug_log(const char *message, ...) {
#ifndef NDEBUG
  va_list list;
  va_start(list, message);
  vprintf(message, list);
  va_end(list);
  putchar('\n');
  fflush(stdout);
#endif
}


bool open_browser(const char *program, const char *args, const char *url) {
  //TODO: why the 100 extra bytes?
  size_t buffer_size = 100;
  if (program != NULL) {
    buffer_size += strlen(program);
  }
  if (args != NULL) {
    buffer_size += strlen(args);
  } else {
    args = "";
  }
  if (url != NULL) {
    buffer_size += strlen(url);
  }
  char buffer[buffer_size];
  sprintf(buffer, "%s %s %s", program, args, url);
  system(buffer);
  return true;
}


static bool extension_supported(const char *name) {
  const char *extensions = glXQueryExtensionsString(display, DefaultScreen(display));
  debug_log(extensions);
  const char *found = strstr(extensions, name);
  if (found) {
    debug_log("found");
    char end = found[strlen(name)];
    if (end == '\0' || end == ' ') {
      return true;
    }
  }
  return false;
}


typedef int (*glXSwapIntervalMESA_t)(int);
static glXSwapIntervalMESA_t p_glXSwapIntervalMESA = NULL;
typedef void (*glXSwapIntervalEXT_t)(Display *, GLXDrawable, int);
static glXSwapIntervalEXT_t p_glXSwapIntervalEXT = NULL;


static void initialize_gl_extensions() {
  if (extension_supported("GLX_MESA_swap_control")) {
    // Intel and AMD and MESA support this one, but not NVIDIA
    p_glXSwapIntervalMESA = (glXSwapIntervalMESA_t)glXGetProcAddressARB((const GLubyte *)"glXSwapIntervalMESA");
  }
  if (extension_supported("GLX_EXT_swap_control")) {
    // This one is supported by NVIDIA, but not Intel or AMD
    p_glXSwapIntervalEXT = (glXSwapIntervalEXT_t)glXGetProcAddressARB((const GLubyte *)"glXSwapIntervalEXT");
  }
}


static void native_reference_window_event_loop(uint8_t pattern[]) {
  // This function should only be called from a child process that isn't yet
  // connected to the X server.
  assert(!display);
  display = XOpenDisplay(NULL);
  assert(display);
  // Initialize GLX.
  int visual_attributes[] = { GLX_RGBA,
                              GLX_DOUBLEBUFFER,
                              GLX_RED_SIZE, 1,
                              GLX_GREEN_SIZE, 1,
                              GLX_BLUE_SIZE, 1,
                              None,
                            };
  XVisualInfo *xvi = glXChooseVisual(display, DefaultScreen(display),
                                     visual_attributes);
  assert(xvi);
  GLXContext context = glXCreateContext(display, xvi, NULL, true);
  assert(context);
  if (!context) {
    debug_log("failed to initialize OpenGL");
    exit(1);
  }

  // Create a window with the correct colormap for GL rendering.
  Colormap colormap = XCreateColormap(display, RootWindow(display, 0),
                                      xvi->visual, AllocNone);
  XSetWindowAttributes xswa;
  memset(&xswa, 0, sizeof(xswa));
  xswa.colormap = colormap;
  // Prevent the window manager from moving this window or putting decorations
  // on it.
  xswa.override_redirect = true;
  Window window = XCreateWindow(display, RootWindow(display, 0), 500, 500,
                                pattern_pixels, 1, 0, xvi->depth, InputOutput,
                                xvi->visual, CWColormap | CWOverrideRedirect,
                                &xswa);
  assert(window);

  XmbSetWMProperties(display, window, "Test window", NULL, NULL, 0, NULL, NULL,
                     NULL);
  XSelectInput(display, window, KeyPressMask | ButtonPressMask | ExposureMask);

  // Initialize GL and extensions.
  bool success = glXMakeCurrent(display, window, context);
  assert(success);
  initialize_gl_extensions();

  // Disable vsync to avoid blocking on swaps. Ideally we would sync to the
  // display's refresh rate and render at the best possible time to achieve low
  // latency and low CPU use while still avoiding tearing. That should be
  // possible, but will be difficult to implement and will depend on driver/
  // compositor support. For now we'll just render as fast as possible to
  // achieve low latency.
  bool disabled_vsync = false;
  if (p_glXSwapIntervalMESA) {
    int ret = p_glXSwapIntervalMESA(0);
    if (ret) {
      debug_log("glXSwapIntervalMESA failed %d", ret);
      exit(1);
    }
    disabled_vsync = true;
  }
  if (!disabled_vsync && p_glXSwapIntervalEXT) {
    p_glXSwapIntervalEXT(display, window, 0);
    disabled_vsync = true;
  }
  if (!disabled_vsync) {
    debug_log("No method of disabling vsync available.");
    exit(1);
  }

  // Draw the pattern on the window before showing it.
  int scrolls = 0;
  int key_downs = 0;
  int esc_presses = 0;
  draw_pattern_with_opengl(pattern, scrolls, key_downs, esc_presses);
  glXSwapBuffers(display, window);
 
  // Show the window.
  XMapRaised(display, window);
  // Override-redirect windows don't automatically gain focus when mapped, so we
  // have to steal it manually.
  XSetInputFocus(display, window, RevertToParent, CurrentTime);

  // Process X11 events in a loop forever unless the parent process dies.
  while (getppid() != 1) {
    while (XPending(display)) {
      XEvent event;
      XNextEvent(display, &event);
      if (event.type == ButtonPress) {
        // This is probably a mousewheel event.
        scrolls++;
      } else if (event.type == KeyPress) {
        if (XkbKeycodeToKeysym(display, event.xkey.keycode, 0, 0) ==
            XK_Escape) {
          esc_presses++;
        }
        key_downs++;
      }
    }
    draw_pattern_with_opengl(pattern, scrolls, key_downs, esc_presses);
    glXSwapBuffers(display, window);
    usleep(1000 * 5);
  }
  XCloseDisplay(display);
}


static pid_t window_process_pid = 0;


bool open_native_reference_window(uint8_t *test_pattern_for_window) {
  if (window_process_pid != 0) {
    debug_log("Native reference window already open");
    return false;
  }
  window_process_pid = fork();
  if (!window_process_pid) {
    // Child process. Throw away the X11 display connection from the parent
    // process; we will create a new one for the child.
    display = NULL;
    native_reference_window_event_loop(test_pattern_for_window);
    exit(0);
  }
  // Parent process. Wait for the child to launch and show its window before
  // returning.
  usleep(1000000 /* 1 second */);
  return true;
}

bool close_native_reference_window() {
  if (window_process_pid == 0) {
    debug_log("Native reference window not open");
    return false;
  }
  int r = kill(window_process_pid, SIGKILL);
  window_process_pid = 0;
  if (r) {
    debug_log("Failed to close native reference window");
    return false;
  }
  return true;
}
