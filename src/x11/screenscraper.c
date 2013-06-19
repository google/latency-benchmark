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
#include <pthread.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>  // XGetPixel, XDestroyImage
#include <X11/keysym.h> // XK_Z
#include <X11/extensions/XTest.h>
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
    XInitThreads();
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


bool send_keystroke_z() {
  if (!display) {
    display = XOpenDisplay(NULL);
    XInitThreads();
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
  event.keycode = XKeysymToKeycode(display, XK_Z);
  event.type = KeyPress;
  XSendEvent(display, focused, True, KeyPressMask, (XEvent*) &event);
  event.type = KeyRelease;
  XSendEvent(display, focused, True, KeyReleaseMask, (XEvent*) &event);
  XSync(display, False);
  return true;
}


bool send_scroll_down(int x, int y) {
  if (!display) {
    display = XOpenDisplay(NULL);
    XInitThreads();
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


bool open_browser(const char *url) {
  size_t buffer_size = strlen(url) + 100;
  char buffer[buffer_size];
  sprintf(buffer, "xdg-open %s", url);
  system(buffer);
  return true;
}


static pthread_t thread;
static bool thread_running = false;
static uint8_t *test_pattern = NULL;


void *control_window_thread(void *ignored) {
  Display *thread_display = XOpenDisplay(NULL);
  assert(thread_display);
  if (!thread_display) {
    return NULL;
  }
  assert(test_pattern);
  Window window = XCreateSimpleWindow(thread_display, RootWindow(thread_display, 0), 100, 100, 100, 100, 0, 0, 0);
  XSelectInput(thread_display, window, KeyPressMask | ButtonPressMask | ExposureMask);
  XmbSetWMProperties(thread_display, window, "Test window", NULL, NULL, 0, NULL, NULL, NULL);
  XMapRaised(thread_display, window);
  while (thread_running) {
    while (XPending(thread_display)) {
      XEvent event;
      XNextEvent(thread_display, &event);
      if (event.type == ButtonPress) {
        // This is probably a mousewheel event.
        test_pattern[4 * 5 + 0]++;
        test_pattern[4 * 5 + 1]++;
        test_pattern[4 * 5 + 2]++;
      } else if (event.type == KeyPress) {
        test_pattern[4 * 4 + 1]++;
      }
    }
    test_pattern[4 * 6 + 0]++;
    test_pattern[4 * 6 + 1]++;
    test_pattern[4 * 6 + 2]++;
    test_pattern[4 * 4 + 0]++;
    for (int i = 0; i < pattern_bytes; i += 4) {
      XSetForeground(thread_display, DefaultGC(thread_display, 0), test_pattern[i + 2] << 16 | test_pattern[i + 1] << 8 | test_pattern[i]);
      XDrawPoint(thread_display, window, DefaultGC(thread_display, 0), i / 4, 0);
    }
    usleep(1000 * 5);
  }
  XCloseDisplay(thread_display);
  return NULL;
}


bool open_control_window(uint8_t *test_pattern_for_window) {
  if (thread_running) {
    return false;
  }
  if (!display) {
    display = XOpenDisplay(NULL);
    XInitThreads();
    if (!display) {
      return false;
    }
  }
  test_pattern = test_pattern_for_window;
  thread_running = true;
  pthread_create(&thread, NULL, &control_window_thread, NULL);
  // Wait for window to show up and be painted.
  usleep(1000 * 1000);
  return true;
}


bool close_control_window() {
  if (!thread_running) {
    return false;
  }
  thread_running = false;
  pthread_join(thread, NULL);
  return true;
}
